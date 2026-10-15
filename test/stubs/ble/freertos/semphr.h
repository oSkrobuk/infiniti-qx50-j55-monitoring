#pragma once

using SemaphoreHandle_t = void *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline void xSemaphoreTake(SemaphoreHandle_t, unsigned int) {}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
