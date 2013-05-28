
#include "ch.h"
#include "hal.h"
#include "touch.h"
#include "terminal.h"
#include "lcd.h"

#include <stdbool.h>

void adccb(ADCDriver *adcp, adcsample_t *buffer, size_t n);
void setup_y_axis(void);
void setup_x_axis(void);


// X+ = PA4 = ADC1-4
#define XP 5
// X- = PA6
#define XN 6
// Y+ = PA5 = ADC1-5
#define YP 4
// Y- = PA7
#define YN 7



/* Total number of channels to be sampled by a single ADC operation.*/
#define ADC_GRP1_NUM_CHANNELS   2

/* Depth of the conversion buffer, channels are sampled four times each.*/
#define ADC_GRP1_BUF_DEPTH      32

/*
 * ADC samples buffer.
 */
static adcsample_t samples[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 4 samples of 2 channels, SW triggered.
 * Channels:    IN11   (48 cycles sample time)
 *              Sensor (192 cycles sample time)
 */
static const ADCConversionGroup adcgrpcfg = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  adccb,
  NULL,
  /* HW dependent part.*/
  0,
  ADC_CR2_SWSTART,
  0,
  ADC_SMPR2_SMP_AN4(ADC_SAMPLE_480) | ADC_SMPR2_SMP_AN5(ADC_SAMPLE_480),
  ADC_SQR1_NUM_CH(ADC_GRP1_NUM_CHANNELS),
  0,
  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN4) | ADC_SQR3_SQ1_N(ADC_CHANNEL_IN5)
};

static adcsample_t avg_y, avg_x;
static volatile uint8_t reading_ready;

void adccb(ADCDriver *adcp, adcsample_t *buffer, size_t n) {

  (void) buffer; (void) n;
  /* Note, only in the ADC_COMPLETE state because the ADC driver fires an
     intermediate callback when the buffer is half full.*/
  if (adcp->state != ADC_COMPLETE) {
    setup_x_axis();
  }
  else {
    /* Calculates the average values from the ADC samples.*/
    avg_y = (samples[0] + samples[2] + samples[4] + samples[6]) / 4;
    avg_x = (samples[1] + samples[3] + samples[5] + samples[7]) / 4;

    /* Starts an asynchronous ADC conversion operation, the conversion
       will be executed in parallel to the current PWM cycle and will
       terminate before the next PWM cycle.*/
    setup_y_axis();

    reading_ready = 1;
  }
}

uint8_t wa_touch_thread[1024];
msg_t
touch_thread(void* arg);

void
touch_init()
{
  terminal_write("in touch init\n");
  chThdCreateStatic(wa_touch_thread, sizeof(wa_touch_thread), NORMALPRIO, touch_thread, NULL);

  adcStart(&ADCD1, NULL);
  adcSTM32EnableTSVREFE();


  /* Starts an asynchronous ADC conversion operation, the conversion
     will be executed in parallel to the current PWM cycle and will
     terminate before the next PWM cycle.*/
  chSysLockFromIsr();
  adcStartConversionI(&ADCD1, &adcgrpcfg, samples, ADC_GRP1_BUF_DEPTH);
  chSysUnlockFromIsr();
}

void setup_axis(u16 rp, u16 rn, u16 bp, u16 bn)
{
  palSetPadMode(GPIOA, rp, PAL_MODE_INPUT_ANALOG);
  palSetPadMode(GPIOA, rn, PAL_MODE_INPUT);
  palSetPadMode(GPIOA, bp, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOA, bn, PAL_MODE_OUTPUT_PUSHPULL);

  palSetPad(GPIOA, bp);
  palClearPad(GPIOA, bn);
}

void setup_y_axis()
{
  setup_axis(YP, YN, XP, XN);
}

void setup_x_axis()
{
  setup_axis(XP, XN, YP, YN);
}



msg_t
touch_thread(void* arg)
{
  (void)arg;

  terminal_write("in touch thread\n");
  while (1) {
    if (reading_ready) {
      reading_ready = 0;
      terminal_clear();
      terminal_write("reading ready\n");
      int i,j;
      for (i = 0; i < ADC_GRP1_NUM_CHANNELS; ++i) {
        terminal_write("channel:\n");
        int avg = 0;
        for (j = 0; j < ADC_GRP1_BUF_DEPTH; ++j) {
          terminal_write_int(samples[i*ADC_GRP1_BUF_DEPTH + j]);
          terminal_write(" ");
          avg += samples[i*ADC_GRP1_BUF_DEPTH + j];
        }
        avg /= ADC_GRP1_BUF_DEPTH;
        terminal_write("avg: ");
        terminal_write_int(avg);
        terminal_write("\n");
      }


      chThdSleepMilliseconds(500);

      chSysLockFromIsr();
      adcStartConversionI(&ADCD1, &adcgrpcfg, samples, ADC_GRP1_BUF_DEPTH);
      chSysUnlockFromIsr();
    }
  }

  return 0;
}
