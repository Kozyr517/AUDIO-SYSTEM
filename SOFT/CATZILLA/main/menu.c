
 // головне меню
//LCD_clear(&lcd);

    // ==========================================
    // ЛІВИЙ СТОВПЧИК (X: 2, W: 90)
    // ==========================================
    // БЛОК 1: "EQ PRESET" (9 символів = 72px) -> X = 2 + (90-72)/2 = 11
    //LCD_DrawRect(&lcd, 2, 2, 90, 18, 1);
    //LCD_print(&lcd, "EQ PRESET", 11, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 2: "FILTERS" (7 символів = 56px) -> X = 2 + (90-56)/2 = 19
    //LCD_DrawRect(&lcd, 2, 23, 90, 18, 1);
   // LCD_print(&lcd, "FILTERS", 19, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 3: "VOLUME/BAL" (10 символів = 80px) -> X = 2 + (90-80)/2 = 7
    //LCD_DrawRect(&lcd, 2, 44, 90, 18, 1);
    //LCD_print(&lcd, "VOLUME/BAL", 7, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // СЕРЕДНІЙ СТОВПЧИК (X: 96, W: 90)
    // ==========================================
    // БЛОК 4: "SPATIAL 3D" (10 символів = 80px) -> X = 96 + (90-80)/2 = 101
    //LCD_DrawRect(&lcd, 96, 2, 90, 18, 1);
    //LCD_print(&lcd, "SPATIAL 3D", 101, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 5: "NIGHT MOD" (9 символів = 72px) -> X = 96 + (90-72)/2 = 105
    //LCD_DrawRect(&lcd, 96, 23, 90, 18, 1);
    //LCD_print(&lcd, "NIGHT MOD", 105, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 6: "PHONO MM" (8 символів = 64px) -> X = 96 + (90-64)/2 = 109
    //LCD_DrawRect(&lcd, 96, 44, 90, 18, 1);
    //LCD_print(&lcd, "PHONO MM", 109, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // ПРАВИЙ БЛОК АНІМАЦІЇ (Квадрат 60x60)
    // ==========================================
    //LCD_DrawRect(&lcd, 190, 2, 60, 60, 1);

    // Вивід даних на дисплей
    //LCD_Update(&lcd);

// фільтри
    // Очищення буфера
   // LCD_clear(&lcd);

    // ==========================================
    // ЛІВИЙ СТОВПЧИК (X: 2, W: 66)
    // ==========================================
    // БЛОК 1: "SILK" (4 символи)
   // LCD_DrawRect(&lcd, 2, 2, 66, 18, 1);
    //LCD_print(&lcd, "SILK", 19, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 2: "PURRITY" (7 символів)
    //LCD_DrawRect(&lcd, 2, 23, 66, 18, 1);
   // LCD_print(&lcd, "PURRITY", 7, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 3: "DRIVE" (5 символів)
   // LCD_DrawRect(&lcd, 2, 44, 66, 18, 1);
    //LCD_print(&lcd, "DRIVE", 15, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // СЕРЕДНІЙ СТОВПЧИК (X: 70, W: 66)
    // ==========================================
    // БЛОК 4: "ATMOS" (5 символів)
    //LCD_DrawRect(&lcd, 70, 2, 66, 18, 1);
   // LCD_print(&lcd, "ATMOS", 83, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 5: "VILVET" (6 символів)
    //LCD_DrawRect(&lcd, 70, 23, 66, 18, 1);
    //LCD_print(&lcd, "VILVET", 79, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 6: "DIRECT" (6 символів)
    //LCD_DrawRect(&lcd, 70, 44, 66, 18, 1);
   // LCD_print(&lcd, "DIRECT", 79, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // ПРАВИЙ БЛОК АНІМАЦІЇ (X: 139, Y: 2, W: 111, H: 60)
    // ==========================================
    //LCD_DrawRect(&lcd, 139, 2, 111, 60, 1);

    // Вивід даних на дисплей
    //LCD_Update(&lcd);

    //vTaskDelay(pdMS_TO_TICKS(1000));
    //}

    //меню еквалайзеров 
    // Очищення буфера
    //LCD_clear(&lcd);

    // ==========================================
    // ЛІВИЙ СТОВПЧИК (X: 2, W: 50)
    // ==========================================
    // БЛОК 1: "POP" (3 символи = 24px)
    //LCD_DrawRect(&lcd, 2, 2, 50, 18, 1);
    //LCD_print(&lcd, "POP", 16, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 2: "ROCK" (4 символи = 32px)
   // LCD_DrawRect(&lcd, 2, 23, 50, 18, 1);
   // LCD_print(&lcd, "ROCK", 12, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 3: "JAZZ" (4 символи = 32px)
    //LCD_DrawRect(&lcd, 2, 44, 50, 18, 1);
    //LCD_print(&lcd, "JAZZ", 11, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // СЕРЕДНІЙ СТОВПЧИК (X: 56, W: 56)
    // ==========================================
    // БЛОК 4: "SYMPH" (5 символів = 40px)
    //LCD_DrawRect(&lcd, 56, 2, 56, 18, 1);
    //LCD_print(&lcd, "SYMPH", 65, 7, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 5: "NATURE" (6 символів = 48px)
    //LCD_DrawRect(&lcd, 56, 23, 56, 18, 1);
   // LCD_print(&lcd, "NATURE", 61, 28, (const uint8_t*)Sinclair_S8x8, 0);

    // БЛОК 6: "BASS" (4 символи = 32px)
    //LCD_DrawRect(&lcd, 56, 44, 56, 18, 1);
    //LCD_print(&lcd, "BASS", 69, 49, (const uint8_t*)Sinclair_S8x8, 0);

    // ==========================================
    // ПРАВИЙ БЛОК АНІМАЦІЇ (X: 115, Y: 2, W: 137, H: 60)
    // ==========================================
    //LCD_DrawRect(&lcd, 115, 2, 137, 60, 1);

    // Вивід даних на дисплей
   // LCD_Update(&lcd);