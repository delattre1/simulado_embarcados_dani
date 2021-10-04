#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"


// Botão
#define BUT1_PIO      PIOD
#define BUT1_PIO_ID   ID_PIOD
#define BUT1_IDX  28
#define BUT1_IDX_MASK (1 << BUT1_IDX)

// Botão 2 pc31
#define BUT2_PIO      PIOC
#define BUT2_PIO_ID   ID_PIOC
#define BUT2_IDX  31
#define BUT2_IDX_MASK (1 << BUT2_IDX)

// Botão 3 pa19
#define BUT3_PIO      PIOA
#define BUT3_PIO_ID   ID_PIOA
#define BUT3_IDX  19
#define BUT3_IDX_MASK (1 << BUT3_IDX)

// LED 1 PA0
#define LED1_PIO      PIOA
#define LED1_PIO_ID   ID_PIOA
#define LED1_IDX      0
#define LED1_IDX_MASK (1 << LED1_IDX)

// LED 2 PC30
#define LED2_PIO      PIOC
#define LED2_PIO_ID   ID_PIOC
#define LED2_IDX      30
#define LED2_IDX_MASK (1 << LED2_IDX)

// LED 3 PB2
#define LED3_PIO      PIOB
#define LED3_PIO_ID   ID_PIOB
#define LED3_IDX      2
#define LED3_IDX_MASK (1 << LED3_IDX)

/*  Interrupt Edge detection is active. */
#define PIO_IT_EDGE             (1u << 6)

volatile char flag_rtt_alarme = 1;
volatile char flag_tc1 = 0;
volatile char flag_tc2 = 0;
volatile char flag_tc3 = 0;
volatile char led1_on  = 1;
volatile char led2_on  = 0;
volatile char led3_on  = 1;


void but1_callback(void);
void but2_callback(void);
void but3_callback(void);

void io_init(void);
void leds_init(void);
void buttons_init(void);
void pin_toggle(Pio *pio, uint32_t mask);

void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
void rtt_set_alarm(void);
static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);



void but1_callback() {
	led1_on = !led1_on;
}

void but2_callback() {
	led2_on = !led2_on;
}

void but3_callback() {
	led3_on = !led3_on;
}

void TC1_Handler(void){
	volatile uint32_t ul_dummy;
	/* Devemos indicar ao TC que a interrupção foi satisfeita.*/
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
	/* Devemos indicar ao TC que a interrupção foi satisfeita.*/
	ul_dummy = tc_get_status(TC1, 0);
	/* Avoid compiler warning */
	UNUSED(ul_dummy);
	/** Muda o estado do LED */
	flag_tc3 = 1;
}

void rtt_set_alarm(void) {
	/*
	* IRQ (interrupção ocorre) apos 4s => 4 pulsos por sengundo (0,25s) -> 16 pulsos são necessários para dar 4s
	* tempo[s] = 0,25 * 16 = 4s
	*/
	uint16_t pllPreScale = (int) (((float) 32768) / 4.0);
	uint32_t irqRTTvalue = 20;   //0.25 * 20 = 5s de intervalo
	RTT_init(pllPreScale, irqRTTvalue);         
}

void RTT_Handler(void) {
	uint32_t ul_status;
	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);
	// /* IRQ due to Time has changed */
	// if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
	// //f_rtt_alarme = false;
	// 	pin_toggle(LED_PIO, LED_IDX_MASK);    // BLINK Led
	// }
	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		flag_rtt_alarme = !flag_rtt_alarme;        // flag RTT alarme
		rtt_set_alarm();
	}
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses) {
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


void leds_init(void) {
	// Configura led
	pmc_enable_periph_clk(LED1_PIO_ID);
	pmc_enable_periph_clk(LED2_PIO_ID);
	pmc_enable_periph_clk(LED3_PIO_ID);
	
	pio_configure(LED1_PIO, PIO_OUTPUT_0, LED1_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED2_PIO, PIO_OUTPUT_0, LED2_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED3_PIO, PIO_OUTPUT_0, LED3_IDX_MASK, PIO_DEFAULT);
	
	pio_clear(LED1_PIO, LED1_IDX_MASK); //inicializa led aceso
	pio_set  (LED2_PIO, LED2_IDX_MASK); //inicializa led apagados
	pio_clear(LED3_PIO, LED3_IDX_MASK); //inicializa led aceso
}

void buttons_init(void) {
	// Inicializa clock do periférico PIO responsavel pelos botoes
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pmc_enable_periph_clk(BUT3_PIO_ID);
	
	// Configura PIO para lidar com o pino do botão como entrada com pull-up
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT3_PIO, PIO_INPUT, BUT3_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	
	pio_set_debounce_filter(BUT1_PIO, BUT1_IDX_MASK, 60);
	pio_set_debounce_filter(BUT2_PIO, BUT2_IDX_MASK, 60);
	pio_set_debounce_filter(BUT3_PIO, BUT3_IDX_MASK, 60);
	
	// Configura interrupção no pino referente ao botao e associa função de callback caso uma interrupção for gerada
	// a função de callback é a: but1_callback()
	pio_handler_set(BUT1_PIO, BUT1_PIO_ID, BUT1_IDX_MASK, PIO_IT_RISE_EDGE, but1_callback);//PIO_IT_EDGE, //PIO_IT_RISE_EDGE, //PIO_IT_FALL_EDGE,);
	pio_handler_set(BUT2_PIO, BUT2_PIO_ID, BUT2_IDX_MASK, PIO_IT_FALL_EDGE, but2_callback);//PIO_IT_EDGE, //PIO_IT_RISE_EDGE, //PIO_IT_FALL_EDGE,);
	pio_handler_set(BUT3_PIO, BUT3_PIO_ID, BUT3_IDX_MASK, PIO_IT_RISE_EDGE, but3_callback);//PIO_IT_EDGE, //PIO_IT_RISE_EDGE, //PIO_IT_FALL_EDGE,);


	// Ativa interrupção e limpa primeira IRQ gerada na ativacao
	pio_enable_interrupt(BUT1_PIO, BUT1_IDX_MASK);
	pio_enable_interrupt(BUT2_PIO, BUT2_IDX_MASK);
	pio_enable_interrupt(BUT3_PIO, BUT3_IDX_MASK);
	
	pio_get_interrupt_status(BUT1_PIO);
	pio_get_interrupt_status(BUT2_PIO);
	pio_get_interrupt_status(BUT3_PIO);
	
	// Configura NVIC para receber interrupcoes do PIO do botao com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_EnableIRQ(BUT3_PIO_ID);
	
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4
	NVIC_SetPriority(BUT2_PIO_ID, 4); // Prioridade 4
	NVIC_SetPriority(BUT3_PIO_ID, 4); // Prioridade 4
}


/**
* Configura TimerCounter (TC) para gerar uma interrupcao no canal (ID_TC e TC_CHANNEL)
* na taxa de especificada em freq.
*/
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter é meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  freq hz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrupçcão no TC canal 0 |  Interrupção no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}


void pin_toggle(Pio *pio, uint32_t mask){
	if(pio_get_output_data_status(pio, mask)) {
		pio_clear(pio, mask);
	}
	else
	pio_set(pio,mask);
}

void io_init(void) {
	leds_init();
	buttons_init();
}

int main (void)
{
	board_init();
	sysclk_init();
	delay_init();
	io_init();
	
	/** Configura timer TC0, canal 1 */
	TC_init(TC0, ID_TC1, 1, 5);    //last argmument is frequency/2
	/** Configura timer TC2, canal 2 */
	TC_init(TC2, ID_TC8, 2, 10);  //last argmument is frequency/2
	
	TC_init(TC1, ID_TC3, 0, 1);   //last argmument is frequency/2
	
	// Init OLED
	gfx_mono_ssd1306_init();
	// Escreve na tela um circulo e um texto
	gfx_mono_draw_filled_circle(20, 16, 16, GFX_PIXEL_SET, GFX_WHOLE);
	gfx_mono_draw_string("Working", 50,16, &sysfont);

	/* Insert application code here, after the board has been initialized. */
	while(1) {
		if (flag_rtt_alarme) {
			gfx_mono_draw_string("Working", 50,16, &sysfont);

			if (flag_tc1 && led1_on) {
				pin_toggle(LED1_PIO, LED1_IDX_MASK);
				flag_tc1 = 0;
			}
			
			if (flag_tc2 && led2_on) {
				pin_toggle(LED2_PIO, LED2_IDX_MASK);
				flag_tc2 = 0;
			}
			
			if (flag_tc3 && led3_on) {
				pin_toggle(LED3_PIO, LED3_IDX_MASK);
				flag_tc3 = 0;
			}
		}
		else {
			gfx_mono_draw_string("Pausado", 50,16, &sysfont);
		}
	}
}
