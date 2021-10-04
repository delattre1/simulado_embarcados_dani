#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"
#include "config_dani.h"

void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);

/**
* Configura TimerCounter (TC) para gerar uma interrupcao no canal (ID_TC e TC_CHANNEL)
* na taxa de especificada em freq.
*/

void rtt_set_alarm(void) {
	/*
	* IRQ (interrup??o ocorre) apos 5s (freq) => 5 pulsos por sengundo (0,20s) -> 25 pulsos s?o necess?rios para dar 5s
	* tempo[s] = 0,25 * 16 = 4s
	*/
	int freq = 5; // Hz
	uint16_t pllPreScale = (int) (((float) 32768) / freq); // 
	uint32_t irqRTTvalue = 25 ;   //0.25 * 20 = 5s de intervalo
	RTT_init(pllPreScale, irqRTTvalue);         
	flag_rtt_alarme = 0;
}

void RTT_Handler(void) {
	uint32_t ul_status;
	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);
	// /* IRQ due to Time has changed */
	
	 if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
		//counter_rtt ++;        // flag RTT alarme
	 }
	 
	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		flag_rtt_alarme = 1;        // flag RTT alarme
		//counter_rtt     = 0;
	}
}

void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses) {
	uint32_t ul_previous_time;
	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);

	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));

	rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN | RTT_MR_RTTINCIEN);
}

void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter ? meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  freq hz e interrup?c?o no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrup?c?o no TC canal 0 |  Interrup??o no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}


void TC1_Handler(void){
	volatile uint32_t ul_dummy;
	/* Devemos indicar ao TC que a interrup??o foi satisfeita.*/
	ul_dummy = tc_get_status(TC0, 1);
	/* Avoid compiler warning */
	UNUSED(ul_dummy);
	/** Muda o estado do LED */
	flag_tc1 = 1;
}

void TC8_Handler(void){
	volatile uint32_t ul_dummy;
	ul_dummy = tc_get_status(TC2, 2);
	/* Avoid compiler warning */
	UNUSED(ul_dummy);
	/** Muda o estado do LED */
	flag_tc2 = 1;
}


void TC3_Handler(void){
	volatile uint32_t ul_dummy;
	/* Devemos indicar ao TC que a interrup??o foi satisfeita.*/
	ul_dummy = tc_get_status(TC1, 0);
	/* Avoid compiler warning */
	UNUSED(ul_dummy);
	/** Muda o estado do LED */
	flag_tc3 = 1;
}

void io_init(void) {
	
	leds_init();
	buttons_init();
	
	// Init OLED
	gfx_mono_ssd1306_init();
	
	//Config TC
	int freq_tc1 = 5;
	int freq_tc2 = 10;
	int freq_tc3 = 1;
		
	TC_init(TC0, ID_TC1, 1, freq_tc1);    // Configura timer TC0, canal 1  freq
	TC_init(TC2, ID_TC8, 2, freq_tc2);    // Configura timer TC2, canal 2 */
	TC_init(TC1, ID_TC3, 0, freq_tc3);    // last argmument is frequency/
	
}

int main (void) {
	board_init();
	sysclk_init();
	delay_init();
	io_init();
	rtt_set_alarm();
	
	gfx_mono_draw_string("Iniciando..",  0, 5, &sysfont); //horizontal, vertical
	delay_s(1);

	while(1) {
		if (flag_tc1 & led1_on & flag_leds_ligados) {
			pin_toggle(LED1_PIO, LED1_IDX_MASK);
			flag_tc1 = 0;
		}
			
		if (flag_tc2 & led2_on & flag_leds_ligados) {
			pin_toggle(LED2_PIO, LED2_IDX_MASK);
			flag_tc2 = 0;
		}
			
		if (flag_tc3 & led3_on & flag_leds_ligados) {
			pin_toggle(LED3_PIO, LED3_IDX_MASK);
			flag_tc3 = 0;
		}
		
		if (flag_rtt_alarme) {
			flag_leds_ligados = !flag_leds_ligados;
			rtt_set_alarm();
		}
	}
}

