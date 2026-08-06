#include "menu.h"
#include "Sinclair_S8x8.h"
#include "driver/spi_master.h"
#include "cat_animation.h"
#include "Font16x16.h"
#include "driver/gpio.h"
#include <string.h>

#define PIN_BTN_1           GPIO_NUM_1   // Кнопка 1 -> Світлодіод 4 (індекс 3)
#define PIN_BTN_2           GPIO_NUM_2   // Кнопка 2 -> Світлодіод 3 (індекс 2)
#define PIN_BTN_3           GPIO_NUM_41  // Кнопка 3 -> Світлодіод 2 (індекс 1)
#define PIN_BTN_4           GPIO_NUM_42  // Кнопка 4 -> Світлодіод 1 (індекс 0)

#define MAIN_MENU_NUM 0
#define FILTERS_MENU_NUM 1
#define EQ_MENU_NUM 2

// Тег один в кожному файлі, щоб розуміти в логах, з якого файлу логи йдуть
static const char *TAG = "MENU";

// Глобальні змінніі, для пізнішого визначення
TypeDef_GP1247AI* lcd;
spi_device_handle_t g_spi_handle;

// Передача даних через SPI
void dma_spi_transmit(uint8_t *data, size_t size) {
    spi_transaction_t trans = {
        .tx_buffer = (void *)data,  // NOLINT
        .length = (8 * size)
    };
    spi_device_transmit(g_spi_handle, &trans);
}

// ==================== ФУНКЦІЯ-АДАПТЕР ДЛЯ КАРТИНОК ====================
// Правильне транспонування та упаковка байтів під формат сторінок GP1247AI
void draw_cat_frame(uint16_t start_x, uint16_t start_y, const uint8_t *h_bitmap, uint16_t w, uint16_t h) {
    uint8_t v_bitmap[384];  // Буфер під кадр 64x48 (64 * (48/8) = 384 байти)
    memset(v_bitmap, 0, sizeof(v_bitmap));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Отримуємо стан пікселя з монохромного бітмапа (горизонтальний формат)
            int h_byte = (y * w + x) / 8;
            int h_bit = 7 - (x % 8);
            uint8_t pixel = (h_bitmap[h_byte] >> h_bit) & 0x01;

            if (pixel) {
                // Розраховуємо сторінку та вертикальний біт для буфера GP1247AI
                int page = y / 8;
                int v_bit = 7 - (y % 8);  // Інверсія біта для виправлення дзеркалення
                int v_byte = page * w + x;

                if (v_byte < sizeof(v_bitmap)) {
                    v_bitmap[v_byte] |= (1 << v_bit);
                }
            }
        }
    }
    // Відправляємо конвертований кадр
    LCD_DrawBitmap(lcd, start_x, start_y, v_bitmap, w, h, 1);
}

// ==================== АНІМАЦІЯ ЗАВАНТАЖЕННЯ ====================
void draw_boot_animation() {
    uint16_t cat_x = (253 - CAT_ANIM_W) / 2;  // ~94
    uint16_t cat_y = (64 - CAT_ANIM_H) / 2;  // ~8

    LCD_clear(lcd);
    LCD_Update(lcd);

    ESP_LOGI(TAG, "Запуск анімації кота...");

    // Крутимо анімацію 5 разів
    for (int loop = 0; loop < 5; loop++) {
        for (int frame = 0; frame < CAT_BOOT_FRAMES; frame++) {
            LCD_clear(lcd);
            draw_cat_frame(cat_x, cat_y, boot_cat_anim[frame], CAT_ANIM_W, CAT_ANIM_H);
            LCD_Update(lcd);
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }

    // Фінальний екран завантаження
    ESP_LOGI(TAG, "Анімацію завершено.");
    LCD_clear(lcd);
    LCD_print(lcd, "CATZILLA", (253 - (8 * 16)) / 2, 10, (const uint8_t*)Font16x16, 0);
    LCD_print(lcd, "ONLINE", (253 - (6 * 16)) / 2, 35, (const uint8_t*)Font16x16, 0);
    LCD_Update(lcd);

    vTaskDelay(pdMS_TO_TICKS(2000));
}

// Функція-адаптер для затримки
void delay_ms_wrapper(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Змінні, для зберігання даних - який пункт меню обрано
uint8_t main_menu_pointer = 0;
uint8_t filters_menu_pointer = 0;
uint8_t eq_menu_pointer = 0;

// Змінна, яка зберігає - яке зараз меню на дисплеї
uint8_t menu_pointer = 0;  // 0 - main menu

// Змінні, необхідні для конкретних пунктів меню (гучність, тощо)
// // //  Мають теж мати дублікати "old_" та порівнюватись за потреби

// Дублікати всіх змінних, для відстеження змін в даних
uint8_t old_menu_pointer = 0;
uint8_t old_main_menu_pointer = 0;
uint8_t old_filters_menu_pointer = 0;
uint8_t old_eq_menu_pointer = 0;

// Масиви, для графіків АЧХ
const uint8_t eq_pop[10]    = {29, 40, 44, 45, 41, 30, 28, 28, 29, 29};
const uint8_t eq_rock[10]   = {45, 40, 23, 19, 26, 39, 47, 50, 50, 50};
const uint8_t eq_jazz[10]   = {40, 38, 32, 33, 31, 35, 39, 41, 43, 44};
const uint8_t eq_symph[10]  = {49, 49, 42, 42, 33, 24, 24, 24, 33, 33};
const uint8_t eq_nature[10] = {33, 33, 33, 33, 33, 33, 33, 33, 33, 33};
const uint8_t eq_bass[10]   = {48, 48, 48, 42, 35, 25, 18, 15, 14, 14};

const uint8_t* eq_presets[6] = {eq_pop, eq_rock, eq_jazz, eq_symph, eq_nature, eq_bass};
const char* eq_names[6] = {"POP", "ROCK", "JAZZ", "SYMPH", "NATURE", "BASS"};

// Функція ініціалізації дисплею + анімацйія завантаження
void display_init() {
    // ІНІЦІАЛІЗАЦІЯ SPI (DMA ВКЛЮЧЕНО)
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2100
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_config = {
        .clock_source = SOC_MOD_CLK_XTAL,
        .clock_speed_hz = 2500000,
        .mode = 0,
        .spics_io_num = PIN_NUM_LCD_CS,
        .queue_size = 300,
        .post_cb = NULL,
        .pre_cb = NULL,
        .flags = SPI_DEVICE_TXBIT_LSBFIRST,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_config, &g_spi_handle));

    lcd->dma_spi_transmit = dma_spi_transmit;
    lcd->delay_ms = delay_ms_wrapper;

    // ІНІЦІАЛІЗАЦІЯ ДИСПЛЕЯ
    gpio_set_level(PIN_NUM_LCD_RS, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    GP1247AI_Ini(lcd);

    draw_boot_animation(lcd);
}

// Функції для виводу кожного виду меню
void set_main_menu() {
    LCD_clear(lcd);

    switch (main_menu_pointer) {
        case 0:
        // LCD_DrawLine ???  Select this box
        break;
        case 1:
        //
        break;
        // case 2,3,4,5
    }

    // ЛІВИЙ СТОВПЧИК (X: 2, W: 90)
    // БЛОК 1: "EQ PRESET" (9 символів = 72px) -> X = 2 + (90-72)/2 = 11
    LCD_DrawRect(lcd, 2, 2, 90, 18, 1);
    LCD_print(lcd, "EQ PRESET", 11, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 2: "FILTERS" (7 символів = 56px) -> X = 2 + (90-56)/2 = 19
    LCD_DrawRect(lcd, 2, 23, 90, 18, 1);
    LCD_print(lcd, "FILTERS", 19, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 3: "VOLUME/BAL" (10 символів = 80px) -> X = 2 + (90-80)/2 = 7
    LCD_DrawRect(lcd, 2, 44, 90, 18, 1);
    LCD_print(lcd, "VOLUME/BAL", 7, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // СЕРЕДНІЙ СТОВПЧИК (X: 96, W: 90)
    // БЛОК 4: "SPATIAL 3D" (10 символів = 80px) -> X = 96 + (90-80)/2 = 101
    LCD_DrawRect(lcd, 96, 2, 90, 18, 1);
    LCD_print(lcd, "SPATIAL 3D", 101, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 5: "NIGHT MOD" (9 символів = 72px) -> X = 96 + (90-72)/2 = 105
    LCD_DrawRect(lcd, 96, 23, 90, 18, 1);
    LCD_print(lcd, "NIGHT MOD", 105, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 6: "PHONO MM" (8 символів = 64px) -> X = 96 + (90-64)/2 = 109
    LCD_DrawRect(lcd, 96, 44, 90, 18, 1);
    LCD_print(lcd, "PHONO MM", 109, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ПРАВИЙ БЛОК АНІМАЦІЇ (Квадрат 60x60)
    LCD_DrawRect(lcd, 190, 2, 60, 60, 1);
    LCD_Update(lcd);
}

void set_filters_menu() {
    LCD_clear(lcd);

    switch (filters_menu_pointer) {
        case 0:
        // LCD_DrawLine ???  Select this box
        break;
        case 1:
        //
        break;
        // case 2,3,4,5
    }

    // ЛІВИЙ СТОВПЧИК (X: 2, W: 66)
    // БЛОК 1: "SILK" (4 символи)
    LCD_DrawRect(lcd, 2, 2, 66, 18, 1);
    LCD_print(lcd, "SILK", 19, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 2: "PURRITY" (7 символів)
    LCD_DrawRect(lcd, 2, 23, 66, 18, 1);
    LCD_print(lcd, "PURRITY", 7, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 3: "DRIVE" (5 символів)
    LCD_DrawRect(lcd, 2, 44, 66, 18, 1);
    LCD_print(lcd, "DRIVE", 15, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // СЕРЕДНІЙ СТОВПЧИК (X: 70, W: 66)
    // БЛОК 4: "ATMOS" (5 символів)
    LCD_DrawRect(lcd, 70, 2, 66, 18, 1);
    LCD_print(lcd, "ATMOS", 83, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 5: "VILVET" (6 символів)
    LCD_DrawRect(lcd, 70, 23, 66, 18, 1);
    LCD_print(lcd, "VILVET", 79, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 6: "DIRECT" (6 символів)
    LCD_DrawRect(lcd, 70, 44, 66, 18, 1);
    LCD_print(lcd, "DIRECT", 79, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ПРАВИЙ БЛОК АНІМАЦІЇ (X: 139, Y: 2, W: 111, H: 60)
    LCD_DrawRect(lcd, 139, 2, 111, 60, 1);
    LCD_Update(lcd);
}

void set_eq_menu() {
    LCD_clear(lcd);

    LCD_print(lcd, eq_names[eq_menu_pointer], 118, 4, (const uint8_t*)Sinclair_S8x8, 0);

    // 3. Пунктирна лінія 0 dB (Y = 29)
    for (int px = 117; px < 251; px += 4) {
        LCD_DrawPixel(lcd, px, 29, 1);
    }

    // 4. Малювання гладкої кривої АЧХ (з'єднання крапок лініями)
    int prev_x = 0;
    int prev_y = 0;

    for (int i = 0; i < 10; i++) {
        int x = 121 + i * 14;
        int y = 62 - eq_presets[eq_menu_pointer][i];

        // Малюємо крапку на частотному вузлі (3x3 пікселі для чіткості)
        LCD_DrawRect(lcd, x - 1, y - 1, 3, 3, 1);

        // Якщо це не перша крапка — з'єднуємо її лінією з попередньою
        if (i > 0) {
            LCD_DrawLine(lcd, prev_x, prev_y, x, y, 1);
        }

        // Запом'ятовуємо координати для наступного відрізка
        prev_x = x;
        prev_y = y;
    }

    switch (eq_menu_pointer) {
        case 0:
        // LCD_DrawLine ???  Select this box
        break;
        case 1:
        //
        break;
        // case 2,3,4,5
    }

    // ЛІВИЙ СТОВПЧИК (X: 2, W: 50)
    // БЛОК 1: "POP" (3 символи = 24px)
    LCD_DrawRect(lcd, 2, 2, 50, 18, 1);
    LCD_print(lcd, "POP", 16, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 2: "ROCK" (4 символи = 32px)
    LCD_DrawRect(lcd, 2, 23, 50, 18, 1);
    LCD_print(lcd, "ROCK", 12, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 3: "JAZZ" (4 символи = 32px)
    LCD_DrawRect(lcd, 2, 44, 50, 18, 1);
    LCD_print(lcd, "JAZZ", 11, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // СЕРЕДНІЙ СТОВПЧИК (X: 56, W: 56)
    // БЛОК 4: "SYMPH" (5 символів = 40px)
    LCD_DrawRect(lcd, 56, 2, 56, 18, 1);
    LCD_print(lcd, "SYMPH", 65, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 5: "NATURE" (6 символів = 48px)
    LCD_DrawRect(lcd, 56, 23, 56, 18, 1);
    LCD_print(lcd, "NATURE", 61, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 6: "BASS" (4 символи = 32px)
    LCD_DrawRect(lcd, 56, 44, 56, 18, 1);
    LCD_print(lcd, "BASS", 69, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ПРАВИЙ БЛОК АЧХ (X: 115, Y: 2, W: 137, H: 60)
    LCD_DrawRect(lcd, 115, 2, 137, 60, 1);
    LCD_Update(lcd);
}

// Функція, яка автоматично оновлює меню. Має викликатись в довічному циклі основної програми
void menu_update() {
    if (  // Перевіряємо, чи щось змінилось. Якщо ні - дисплей не буде оновлюватись.
        menu_pointer != old_menu_pointer || main_menu_pointer != old_main_menu_pointer ||
        filters_menu_pointer != old_filters_menu_pointer || eq_menu_pointer != old_eq_menu_pointer
    ) {
        // Запам'ятовуємо все що змінилось щоб потім порівнювати вже з ним
        old_eq_menu_pointer = eq_menu_pointer;
        old_filters_menu_pointer = filters_menu_pointer;
        old_main_menu_pointer = main_menu_pointer;
        old_menu_pointer = menu_pointer;
        // Оновлюємо меню
        switch (menu_pointer) {
            case MAIN_MENU_NUM:
                set_main_menu();
                break;
            case FILTERS_MENU_NUM:
                set_eq_menu();
                break;
            case EQ_MENU_NUM:
                set_filters_menu();
                break;
        }
    }
}

void buttons_in_menu_process(uint32_t butt_num) {
    switch (butt_num) {
        case PIN_BTN_1:  // UP
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    if (main_menu_pointer > 0) main_menu_pointer -= 1;
                    break;
                case FILTERS_MENU_NUM:
                    if (filters_menu_pointer > 0) filters_menu_pointer -= 1;
                    break;
                case EQ_MENU_NUM:
                    if (eq_menu_pointer > 0) eq_menu_pointer -= 1;
                    break;
            }
            break;
        case PIN_BTN_2:  // DOWN
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    if (main_menu_pointer < 5) main_menu_pointer += 1;
                    break;
                case FILTERS_MENU_NUM:
                    if (filters_menu_pointer < 5) filters_menu_pointer += 1;
                    break;
                case EQ_MENU_NUM:
                    if (eq_menu_pointer < 5) eq_menu_pointer += 1;
                    break;
            }
            break;
        case PIN_BTN_3:  // OK
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    switch (main_menu_pointer) {
                        case 0:
                            menu_pointer = 1;
                            break;
                        case 1:
                            menu_pointer = 2;
                            break;
                    }
                    break;
                case FILTERS_MENU_NUM:
                    ESP_LOGW(TAG, "FUTURE UPDATE");
                    break;
                case EQ_MENU_NUM:
                    ESP_LOGW(TAG, "FUTURE UPDATE");
                    break;
            }
            break;
        case PIN_BTN_4:  // ESC
            switch (menu_pointer) {
                case MAIN_MENU_NUM:
                    ESP_LOGW(TAG, "FUTURE UPDATE");
                    break;
                case FILTERS_MENU_NUM:
                    menu_pointer = 0;
                    break;
                case EQ_MENU_NUM:
                    menu_pointer = 0;
                    break;
            }
            break;
    }
}

static QueueHandle_t gpio_event_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t pin_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &pin_num, NULL);
}

static void button_task(void* arg) {
    uint32_t io_num;
    TickType_t last_press_time = 0;

    while (1) {
        if (xQueueReceive(gpio_event_queue, &io_num, portMAX_DELAY)) {
            TickType_t now = xTaskGetTickCount();

            if (now - last_press_time > pdMS_TO_TICKS(50)) {
                last_press_time = now;
                ESP_LOGI(TAG, "Pressed button num: %ld", io_num);
                // В майбутньому будуть різні функції якщо музика грає та якщо ні
                buttons_in_menu_process(io_num);
            }
        }
    }
}

void buttons_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_1) | (1ULL << PIN_BTN_2) | (1ULL << PIN_BTN_3) | (1ULL << PIN_BTN_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);

    gpio_event_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(button_task, "button_task", 3072, NULL, 10, NULL);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(PIN_BTN_1, gpio_isr_handler, (void*) PIN_BTN_1);  // NOLINT
    gpio_isr_handler_add(PIN_BTN_2, gpio_isr_handler, (void*) PIN_BTN_2);  // NOLINT
    gpio_isr_handler_add(PIN_BTN_3, gpio_isr_handler, (void*) PIN_BTN_3);  // NOLINT
    gpio_isr_handler_add(PIN_BTN_4, gpio_isr_handler, (void*) PIN_BTN_4);  // NOLINT
}
