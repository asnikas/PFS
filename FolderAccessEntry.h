#pragma once

struct FolderAccessEntry{
    int uid;
    bool can_read, can_write, can_traverse;

    FolderAccessEntry();

    FolderAccessEntry(int uid, bool can_read, bool can_write, bool can_traverse);
};
