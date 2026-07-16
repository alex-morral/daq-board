-- audio_fir_top.vhd
-- Top-level per processament d'audio en temps real amb filtre FIR
--
-- Conecta l'audio del codec ADAU1761 (via I2S) amb el filtre FIR,
-- i permet controlar els coeficients des del ARM via AXI-Lite.
--
-- Flux del senyal:
--   MIC IN -> ADAU1761 -> I2S -> i2s_serdes -> FIR -> i2s_serdes -> I2S -> ADAU1761 -> HP OUT
--                                                 ^
--                                                 | AXI-Lite (coeficients des de Python)
--                                              ARM (PS)
--
-- Font externa (mode extern): el PS escriu mostres al FIR via AXI (reg 0x04),
-- p.ex. mostres que arriben des d'una placa DAQ per UART. Aixi el mateix FIR
-- filtra audio (I2S) o dades externes (AXI) segons el bit de control.
--
-- Mapa de registres (AXI-Lite, base 0x40000000):
--   0x00 : Control (bit 0: enable, bit 1: reset FIR, bit 2: passthrough, bit 3: extern)
--   0x04 : Din - escriu una mostra de 16 bits al FIR (mode extern)
--   0x08 : Dout - ultima mostra filtrada (nomes lectura)
--   0x10 : Coeficient 0
--   0x14 : Coeficient 1
--   0x18 : Coeficient 2
--   0x1C : Coeficient 3
--
-- Modes:
--   enable=0: silenci (no hi ha sortida d'audio)
--   enable=1, passthrough=0: audio filtrat pel FIR
--   enable=1, passthrough=1: audio sense filtrar (per verificar que l'audio funciona)
--   extern=1: les mostres del FIR venen d'AXI (0x04), no de l'I2S

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity audio_fir_top is
    generic (
        DATA_WIDTH : integer := 16;
        COEF_WIDTH : integer := 16;
        NUM_TAPS   : integer := 4
    );
    port (
        -- AXI-Lite (control des del ARM)
        s_axi_aclk    : in  std_logic;
        s_axi_aresetn : in  std_logic;

        s_axi_awaddr  : in  std_logic_vector(4 downto 0);
        s_axi_awvalid : in  std_logic;
        s_axi_awready : out std_logic;

        s_axi_wdata   : in  std_logic_vector(31 downto 0);
        s_axi_wstrb   : in  std_logic_vector(3 downto 0);
        s_axi_wvalid  : in  std_logic;
        s_axi_wready  : out std_logic;

        s_axi_bresp   : out std_logic_vector(1 downto 0);
        s_axi_bvalid  : out std_logic;
        s_axi_bready  : in  std_logic;

        s_axi_araddr  : in  std_logic_vector(4 downto 0);
        s_axi_arvalid : in  std_logic;
        s_axi_arready : out std_logic;

        s_axi_rdata   : out std_logic_vector(31 downto 0);
        s_axi_rresp   : out std_logic_vector(1 downto 0);
        s_axi_rvalid  : out std_logic;
        s_axi_rready  : in  std_logic;

        -- I2S (audio codec ADAU1761)
        bclk      : in  std_logic;
        lrclk     : in  std_logic;
        sdata_i   : in  std_logic;
        sdata_o   : out std_logic;

        -- ADAU1761 address pins
        codec_addr : out std_logic_vector(1 downto 0)
    );
end audio_fir_top;

architecture rtl of audio_fir_top is

    -- Registres AXI-Lite
    signal ctrl_reg  : std_logic_vector(31 downto 0) := (others => '0');
    signal dout_reg  : std_logic_vector(31 downto 0) := (others => '0');
    signal coef_regs : std_logic_vector(NUM_TAPS*32-1 downto 0) := (others => '0');

    -- Senyals de control
    signal ctrl_enable      : std_logic;
    signal ctrl_passthrough : std_logic;
    signal ctrl_ext         : std_logic;   -- 1 = mostres des d'AXI (extern)

    -- Font externa de mostres (des del PS via AXI, reg 0x04)
    signal ext_din : std_logic_vector(DATA_WIDTH-1 downto 0) := (others => '0');
    signal ext_en  : std_logic := '0';     -- pols d'un cicle en escriure 0x04

    -- Senyals I2S
    signal i2s_rst      : std_logic;
    signal i2s_rx_data  : std_logic_vector(23 downto 0);
    signal i2s_rx_valid : std_logic;
    signal i2s_tx_data  : std_logic_vector(23 downto 0);
    signal i2s_tx_load  : std_logic;

    -- Senyals FIR
    signal fir_rst    : std_logic;
    signal fir_en     : std_logic;
    signal fir_din    : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal fir_coeffs : std_logic_vector(NUM_TAPS*COEF_WIDTH-1 downto 0);
    signal fir_dout   : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal fir_valid  : std_logic;

    -- Registre de sortida audio (el que s'envia al codec)
    signal tx_data_reg : std_logic_vector(23 downto 0) := (others => '0');

    -- Senyals AXI internes
    signal aw_ready : std_logic := '0';
    signal w_ready  : std_logic := '0';
    signal b_valid  : std_logic := '0';
    signal ar_ready : std_logic := '0';
    signal r_valid  : std_logic := '0';
    signal r_data   : std_logic_vector(31 downto 0) := (others => '0');
    signal rd_addr  : std_logic_vector(4 downto 0) := (others => '0');

begin

    -- Bits de control
    ctrl_enable      <= ctrl_reg(0);
    ctrl_passthrough <= ctrl_reg(2);
    ctrl_ext         <= ctrl_reg(3);

    -- Adreca I2C del codec ADAU1761
    codec_addr <= "11";

    -------------------------------------------------------
    -- I2S Serialitzador/Deserialitzador
    -------------------------------------------------------
    i2s_rst <= not s_axi_aresetn;

    i2s_inst: entity work.i2s_serdes
        generic map (
            BIT_DEPTH => 24
        )
        port map (
            clk      => s_axi_aclk,
            rst      => i2s_rst,
            bclk     => bclk,
            lrclk    => lrclk,
            sdata_i  => sdata_i,
            sdata_o  => sdata_o,
            rx_data  => i2s_rx_data,
            rx_valid => i2s_rx_valid,
            tx_data  => i2s_tx_data,
            tx_load  => i2s_tx_load
        );

    -------------------------------------------------------
    -- Filtre FIR
    -------------------------------------------------------
    fir_rst <= ctrl_reg(1) or (not s_axi_aresetn);

    fir_inst: entity work.fir_filter
        generic map (
            DATA_WIDTH => DATA_WIDTH,
            COEF_WIDTH => COEF_WIDTH,
            NUM_TAPS   => NUM_TAPS
        )
        port map (
            clk    => s_axi_aclk,
            rst    => fir_rst,
            en     => fir_en,
            din    => fir_din,
            coeffs => fir_coeffs,
            dout   => fir_dout,
            valid  => fir_valid
        );

    -- Font del FIR: AXI (extern) si ctrl_ext=1, si no la cadena I2S
    fir_din <= ext_din when ctrl_ext = '1'
               else i2s_rx_data(23 downto 8);   -- I2S 24 -> 16 bits (MSBs)

    fir_en  <= ext_en when ctrl_ext = '1'
               else (i2s_rx_valid and ctrl_enable and (not ctrl_passthrough));

    -- Extreure coeficients dels registres AXI
    gen_coeffs: for i in 0 to NUM_TAPS-1 generate
        fir_coeffs((i+1)*COEF_WIDTH-1 downto i*COEF_WIDTH) <=
            coef_regs(i*32+COEF_WIDTH-1 downto i*32);
    end generate;

    -------------------------------------------------------
    -- Seleccio de sortida: passthrough o filtrat
    -------------------------------------------------------
    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                tx_data_reg <= (others => '0');
                dout_reg    <= (others => '0');
            else
                -- Passthrough: audio directe sense filtrar
                if ctrl_passthrough = '1' and i2s_rx_valid = '1' then
                    tx_data_reg <= i2s_rx_data;
                end if;

                -- Filtrat: sortida del FIR padejada de 16 a 24 bits.
                -- dout_reg es llegeix per AXI (0x08) tant en I2S com en mode extern.
                if fir_valid = '1' then
                    tx_data_reg <= fir_dout & x"00";
                    dout_reg(DATA_WIDTH-1 downto 0) <= fir_dout;
                end if;

                -- Silenci quan esta desactivat (nomes afecta l'audio I2S)
                if ctrl_enable = '0' then
                    tx_data_reg <= (others => '0');
                end if;
            end if;
        end if;
    end process;

    -- Conectar sortida al I2S
    i2s_tx_data <= tx_data_reg;

    -------------------------------------------------------
    -- AXI-Lite: Canal d'escriptura
    -------------------------------------------------------

    -- Capturar adreca d'escriptura
    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                aw_ready <= '0';
            elsif s_axi_awvalid = '1' and aw_ready = '0' then
                aw_ready <= '1';
            else
                aw_ready <= '0';
            end if;
        end if;
    end process;

    -- Escriure registres
    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            ext_en <= '0';                       -- pols d'un cicle per defecte
            if s_axi_aresetn = '0' then
                w_ready   <= '0';
                ctrl_reg  <= (others => '0');
                coef_regs <= (others => '0');
                ext_din   <= (others => '0');
            elsif s_axi_wvalid = '1' and w_ready = '0' then
                w_ready <= '1';
                case s_axi_awaddr(4 downto 2) is
                    when "000" => ctrl_reg <= s_axi_wdata;                       -- 0x00
                    when "001" =>                                                -- 0x04 : Din
                        ext_din <= s_axi_wdata(DATA_WIDTH-1 downto 0);
                        ext_en  <= '1';                                          -- dispara el FIR (1 cicle)
                    -- "010" (0x08) dout, nomes lectura
                    when "100" => coef_regs(31  downto 0)  <= s_axi_wdata;       -- 0x10
                    when "101" => coef_regs(63  downto 32) <= s_axi_wdata;       -- 0x14
                    when "110" => coef_regs(95  downto 64) <= s_axi_wdata;       -- 0x18
                    when "111" => coef_regs(127 downto 96) <= s_axi_wdata;       -- 0x1C
                    when others => null;
                end case;
            else
                w_ready <= '0';
            end if;
        end if;
    end process;

    -- Resposta d'escriptura
    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                b_valid <= '0';
            elsif aw_ready = '1' and w_ready = '1' then
                b_valid <= '1';
            elsif s_axi_bready = '1' and b_valid = '1' then
                b_valid <= '0';
            end if;
        end if;
    end process;

    -------------------------------------------------------
    -- AXI-Lite: Canal de lectura
    -------------------------------------------------------

    -- Capturar adreca de lectura
    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                ar_ready <= '0';
                rd_addr  <= (others => '0');
            elsif s_axi_arvalid = '1' and ar_ready = '0' then
                ar_ready <= '1';
                rd_addr  <= s_axi_araddr;
            else
                ar_ready <= '0';
            end if;
        end if;
    end process;

    -- Llegir registres
    process(s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                r_valid <= '0';
                r_data  <= (others => '0');
            elsif ar_ready = '1' then
                r_valid <= '1';
                case rd_addr(4 downto 2) is
                    when "000" => r_data <= ctrl_reg;
                    when "010" => r_data <= dout_reg;
                    when "100" => r_data <= coef_regs(31  downto 0);
                    when "101" => r_data <= coef_regs(63  downto 32);
                    when "110" => r_data <= coef_regs(95  downto 64);
                    when "111" => r_data <= coef_regs(127 downto 96);
                    when others => r_data <= (others => '0');
                end case;
            elsif s_axi_rready = '1' and r_valid = '1' then
                r_valid <= '0';
            end if;
        end if;
    end process;

    -- Conectar senyals internes als ports de sortida
    s_axi_awready <= aw_ready;
    s_axi_wready  <= w_ready;
    s_axi_bresp   <= "00";
    s_axi_bvalid  <= b_valid;
    s_axi_arready <= ar_ready;
    s_axi_rdata   <= r_data;
    s_axi_rresp   <= "00";
    s_axi_rvalid  <= r_valid;

end rtl;
