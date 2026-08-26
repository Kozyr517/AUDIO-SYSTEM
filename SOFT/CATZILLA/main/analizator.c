#include "analizator.h"

#define IS_INPUT_SIGNAL_TIMEOUT 10 // if input dont change more than 5m set flag to 0 300 is 5m

uint8_t is_input_sig_flag = 0;

static TaskHandle_t read_signal_task_handle;
static TaskHandle_t fft_task_handle;
static SemaphoreHandle_t fft_process_sem;
extern QueueHandle_t g_fft_process_result_queue;
i2s_chan_handle_t rx_handle;

portMUX_TYPE update_lock = portMUX_INITIALIZER_UNLOCKED;

int32_t rx_buff[2][RX_SIZE * 2];
bool buffer_switch = false;
char txt_buff[100];
uint32_t g_pcm_speed = 0;

int N = RX_SIZE;
// Window coefficients
__attribute__((aligned(16))) float wind[RX_SIZE];
// working complex array
__attribute__((aligned(16))) float y_cf[RX_SIZE * 2];
// Pointers to result arrays
float *y1_cf = &y_cf[0];
float *y2_cf = &y_cf[RX_SIZE];

void is_input_signal(uint8_t *src, size_t size) {
	static uint64_t old_state = 0;
	static uint64_t state = 0;
	static uint16_t frame_skip = 0;
	static uint16_t old_state_count = 0;

	if (frame_skip++ > 46) { // do calc for every 1 sec 50 fps per sec

		frame_skip = 0;
		for (size_t i = 0; i < size; i++) {
			state += src[i];
		}

		state /= size;

		if (state != old_state) {
			old_state = state;
			old_state_count = 0;
			if (is_input_sig_flag == 0) {
				taskENTER_CRITICAL(&update_lock);
				is_input_sig_flag = 1;
				taskEXIT_CRITICAL(&update_lock);
			}
		} else {
			if (old_state_count < IS_INPUT_SIGNAL_TIMEOUT) {
				old_state_count++;
			} else {
				if (is_input_sig_flag == 1) {
					taskENTER_CRITICAL(&update_lock);
					is_input_sig_flag = 0;
					taskEXIT_CRITICAL(&update_lock);
				}
			}
		}
	}
}

void get_signal_process(void *argv) {
	esp_err_t ret;
	size_t r_bytes = 0;
	i2s_channel_enable(rx_handle);
	vTaskDelay(1);

	for (;;) {
		ret = i2s_channel_read(rx_handle, (void *)rx_buff[buffer_switch], RX_SIZE * 8, &r_bytes, 50);

		if (ret == ESP_OK) {
			buffer_switch = !buffer_switch;
			xSemaphoreGive(fft_process_sem);
		} else {
			vTaskDelay(1000);
		}
	}
}

void analiz_process(void *argv) {
	uint8_t colum[COLUM_SIZE] = {0};

	for (;;) {

		if (xSemaphoreTake(fft_process_sem, portMAX_DELAY) == pdTRUE) {
			// Convert two input vectors to one complex vector
			for (int i = 0; i < N; i++) {
				y_cf[i * 2 + 0] = ((float)rx_buff[!buffer_switch][i * 2 + 0] / 10000000) * wind[i];
				y_cf[i * 2 + 1] = ((float)rx_buff[!buffer_switch][i * 2 + 1] / 10000000) * wind[i];
			}

			// FFT
			dsps_fft2r_fc32(y_cf, N);
			// Bit reverse
			dsps_bit_rev_fc32(y_cf, N);
			// Convert one complex vector to two complex vectors
			dsps_cplx2reC_fc32(y_cf, N);

			for (int i = 1; i < N / 4; i = i + 2) {
				y1_cf[i] = (10 * log10f((y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] + y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]) / N));
				y2_cf[i] = (10 * log10f((y2_cf[i * 2 + 0] * y2_cf[i * 2 + 0] + y2_cf[i * 2 + 1] * y2_cf[i * 2 + 1]) / N));
			}

			for (size_t i = 0; i < COLUM_SIZE / 2; i++) {
				if (y1_cf[i * 2 + 1] < 0) {
					colum[i + COLUM_SIZE / 2] = 0;
				} else {
					if (y1_cf[i * 2 + 1] > 63) {
						colum[i + COLUM_SIZE / 2] = 63;
					} else {
						colum[i + COLUM_SIZE / 2] = (uint8_t)y1_cf[i * 2 + 1];
					}
				}
			}

			for (size_t i = 0; i < COLUM_SIZE / 2; i++) {
				if (y2_cf[i * 2 + 1] < 0) {
					colum[COLUM_SIZE / 2 - 1 - i] = 0;
				} else {
					if (y2_cf[i * 2 + 1] > 63) {
						colum[COLUM_SIZE / 2 - 1 - i] = 63;
					} else {
						colum[COLUM_SIZE / 2 - 1 - i] = (uint8_t)y2_cf[i * 2 + 1];
					}
				}
			}
			xQueueSend(g_fft_process_result_queue, colum, 0);
			is_input_signal(colum, COLUM_SIZE);
		}
	}
}

void analizator_init(void) {
	esp_err_t ret;

	i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_SLAVE);

	i2s_new_channel(&chan_cfg, NULL, &rx_handle);

	i2s_std_config_t std_cfg = {
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
		.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
		.gpio_cfg =
			{
				.mclk = I2S_GPIO_UNUSED,
				.bclk = I2S_BCK,
				.ws = I2S_LRCK,
				.dout = I2S_GPIO_UNUSED,
				.din = I2S_DIN,
				.invert_flags =
					{
						.mclk_inv = false,
						.bclk_inv = false,
						.ws_inv = false,
					},
			},
	};

	i2s_channel_init_std_mode(rx_handle, &std_cfg);

	ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
	if (ret != ESP_OK) {
		return;
	}

	// Generate blackman window
	dsps_wind_blackman_f32(wind, N);

	fft_process_sem = xSemaphoreCreateBinary();

	xTaskCreate(&get_signal_process, "get_signal_process", 4096, NULL, tskIDLE_PRIORITY, &read_signal_task_handle);
	xTaskCreate(&analiz_process, "analiz_process", 4096, NULL, tskIDLE_PRIORITY, &fft_task_handle);
}

void clear_byffer(void) {
	for (size_t i = 0; i < RX_SIZE * 2; i++) {
		rx_buff[0][i] = 0;
		rx_buff[1][i] = 0;
	}
}
