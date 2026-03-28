#pragma once

#include "pkgi.hpp"

class LogViewer
{
public:
    // input: raw snapshot BEFORE pkgi.cpp zeros it (for hold-to-scroll).
    void render(const pkgi_input& input);

    bool is_closed() const { return _closed; }
    void close() { _closed = true; }

private:
    int  _selected{0};
    bool _closed{false};

    // hold-to-scroll: simple frame counter
    int _scroll_counter{0};
    static constexpr int ScrollThreshold = 8; // 8 frames ≈ 133 ms at 60 fps
};