#pragma once

#include <map>
#include <string>

#include "Arduino.h"

// Файловая система в памяти вместо LittleFS.
//
// Позволяет прогонять под хостом настоящий код ConfigManager, AlertManager и
// FsUtils — включая атомарную запись через временный файл и переименование.
// Флаги mock_fs_* дают воспроизвести отказы флеша, которые на живом железе
// поймать нечем: нехватку места и неудачный rename

// Содержимое виртуальной ФС: путь → байты файла
inline std::map<std::string, std::string> mock_fs_files;

// Предел размера файла при записи. SIZE_MAX — без ограничения;
// меньшее значение имитирует нехватку места на разделе
inline size_t mock_fs_write_limit = SIZE_MAX;

// Если true — rename() отказывает всегда
inline bool mock_fs_rename_always_fails = false;

// Если true — rename() отказывает только когда целевой файл уже существует.
// Так ведут себя реализации, не умеющие заменять файл при переименовании —
// ради них в FsUtils есть запасной путь «удалить цель и повторить»
inline bool mock_fs_rename_fails_if_exists = false;

// Сбросить ФС и все флаги отказов в исходное состояние
inline void mock_fs_reset()
{
    mock_fs_files.clear();
    mock_fs_write_limit            = SIZE_MAX;
    mock_fs_rename_always_fails    = false;
    mock_fs_rename_fails_if_exists = false;
}

class File : public Stream {
public:
    File() {}
    File(std::string *data, bool writable)
        : data_(data), writable_(writable) {}

    explicit operator bool() const { return data_ != nullptr; }

    void close()
    {
        data_ = nullptr;
        pos_  = 0;
    }

    size_t size() const { return data_ ? data_->size() : 0; }

    // ── Stream ──

    int available() override
    {
        if (!data_ || pos_ >= data_->size()) return 0;
        return static_cast<int>(data_->size() - pos_);
    }

    int read() override
    {
        if (!data_ || pos_ >= data_->size()) return -1;
        return static_cast<unsigned char>((*data_)[pos_++]);
    }

    int peek() override
    {
        if (!data_ || pos_ >= data_->size()) return -1;
        return static_cast<unsigned char>((*data_)[pos_]);
    }

    // ── Print ──

    size_t write(uint8_t c) override
    {
        if (!data_ || !writable_) return 0;
        if (data_->size() >= mock_fs_write_limit) return 0;
        data_->push_back(static_cast<char>(c));
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override
    {
        size_t written = 0;
        while (written < size) {
            if (write(buffer[written]) == 0) break;
            written++;
        }
        return written;
    }

private:
    // Указатель в mock_fs_files: std::map не инвалидирует ссылки на значения
    // при вставке других ключей, поэтому открытый файл переживает создание
    // соседнего (именно это делает атомарная запись через .tmp)
    std::string *data_     = nullptr;
    size_t       pos_      = 0;
    bool         writable_ = false;
};

class MockLittleFS {
public:
    bool begin(bool = false) { return true; }
    void end() {}
    bool format()
    {
        mock_fs_files.clear();
        return true;
    }

    bool exists(const char *path)
    {
        return mock_fs_files.count(path) != 0;
    }

    File open(const char *path, const char *mode = "r")
    {
        const bool writable = (mode != nullptr && (mode[0] == 'w' || mode[0] == 'a'));

        if (writable) {
            std::string &data = mock_fs_files[path];
            if (mode[0] == 'w') data.clear();
            return File(&data, true);
        }

        auto it = mock_fs_files.find(path);
        if (it == mock_fs_files.end()) return File();
        return File(&it->second, false);
    }

    bool remove(const char *path)
    {
        return mock_fs_files.erase(path) != 0;
    }

    bool rename(const char *from, const char *to)
    {
        if (mock_fs_rename_always_fails) return false;
        if (mock_fs_rename_fails_if_exists && mock_fs_files.count(to) != 0) return false;

        auto it = mock_fs_files.find(from);
        if (it == mock_fs_files.end()) return false;

        mock_fs_files[to] = it->second;
        mock_fs_files.erase(it);
        return true;
    }
};

inline MockLittleFS LittleFS;
