#pragma once

class LogViewer
{
public:
    void render();

    bool is_closed() const { return _closed; }
    void close() { _closed = true; }

private:
    int _selected{0};
    bool _closed{false};
};