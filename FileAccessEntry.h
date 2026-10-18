#pragma once

struct FileAccessEntry{
    int uid;
    bool can_read, can_write;

    FileAccessEntry();

    FileAccessEntry(int uid, bool can_read, bool can_write);
};
