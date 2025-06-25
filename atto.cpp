#include "editor.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace ftxui;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: atto <filename>" << std::endl;
        return 1;
    }

    Editor editor(argv[1]);
    auto screen = ScreenInteractive::Fullscreen();

    auto component = Renderer([&] {
        // --- Config Check ---
        bool vim_mode_enabled = editor.config.value("vim_mode", true);

        // --- Layout & Dimension Calculations ---
        const int gutter_width = 5;
        editor.terminal_height = screen.dimy() - 2; // -1 for status bar, -1 for title
        editor.terminal_width = screen.dimx();

        // --- Clean Title Bar ---
        auto title_bar = text("Atto") | bold;

        // --- Line Number Gutter Rendering ---
        std::vector<Element> line_numbers;
        int y_end = std::min((int)editor.buffer.size(), editor.scroll_offset_y + editor.terminal_height);
        for (int i = editor.scroll_offset_y; i < y_end; ++i) {
            std::stringstream ss;
            ss << std::setw(gutter_width) << std::left << i + 1;
            line_numbers.push_back(text(ss.str()) | color(Color::GrayDark));
        }
        auto gutter = vbox(std::move(line_numbers));

        // --- Text Area Rendering with Manual Cursor ---
        std::vector<Element> text_lines;
        for (int i = editor.scroll_offset_y; i < y_end; ++i) {
            std::string line = editor.buffer[i];
            std::string visible_line;
            int available_width = editor.terminal_width - gutter_width;
            if (line.length() > editor.scroll_offset_x) {
                visible_line = line.substr(editor.scroll_offset_x, available_width);
            }

            // --- Manual Cursor Rendering Logic ---
            if (vim_mode_enabled && i == editor.cursor_y) {
                int cursor_screen_x = editor.cursor_x - editor.scroll_offset_x;

                if (cursor_screen_x >= 0 && cursor_screen_x <= visible_line.length()) {
                    std::string before_cursor = visible_line.substr(0, cursor_screen_x);
                    std::string at_cursor = (cursor_screen_x < visible_line.length()) ? visible_line.substr(cursor_screen_x, 1) : " ";
                    std::string after_cursor = (cursor_screen_x + 1 < visible_line.length()) ? visible_line.substr(cursor_screen_x + 1) : "";

                    text_lines.push_back(hbox({
                        text(before_cursor),
                        text(at_cursor) | inverted,
                        text(after_cursor)
                    }));
                } else {
                    text_lines.push_back(text(visible_line));
                }
            } else {
                text_lines.push_back(text(visible_line));
            }
        }
        auto text_area = vbox(std::move(text_lines));


        auto main_pane = hbox({
            gutter,
            text_area | flex,
        });


        Element status_line;
        if (vim_mode_enabled && editor.mode == Mode::Command) {
            status_line = text(":" + editor.command_buffer);
        } else if (!editor.status_message.empty()) {
            status_line = text(editor.status_message);
        } else {
            std::string filename_str = editor.filename.value_or("Untitled");
            Elements status_parts;

            if (vim_mode_enabled) {
                status_parts.push_back(text(editor.mode_str()) | bold | size(WIDTH, EQUAL, 8));
                status_parts.push_back(separator());
            }

            status_parts.push_back(text(filename_str));
            status_parts.push_back(filler());
            status_parts.push_back(separator());
            status_parts.push_back(text(" Ln " + std::to_string(editor.cursor_y + 1) + ", Col " + std::to_string(editor.cursor_x + 1) + " "));

            status_line = hbox(std::move(status_parts));
        }

        auto status_bar = status_line | color(Color::Black) | bgcolor(Color::White);

        return vbox({
            title_bar,
            main_pane | flex,
            status_bar,
        });
    });

    auto event_handler = CatchEvent(component, [&](Event event) {
        if (!editor.is_running) {
            screen.Exit();
            return true;
        }

        if (editor.mode != Mode::Command) {
            editor.status_message.clear();
        }

        bool vim_mode_enabled = editor.config.value("vim_mode", true);

        if (vim_mode_enabled) {
            // VIM MODE EVENT HANDLING
            if (editor.mode == Mode::Normal) {
                if (event == Event::Character(':')) { editor.set_mode(Mode::Command); editor.command_buffer.clear(); return true; }
                if (event == Event::Character('i')) { editor.set_mode(Mode::Insert); return true; }
                if (event == Event::ArrowUp || event == Event::Character('k')) { editor.move_up(); return true; }
                if (event == Event::ArrowDown || event == Event::Character('j')) { editor.move_down(); return true; }
                if (event == Event::ArrowLeft || event == Event::Character('h')) { editor.move_left(); return true; }
                if (event == Event::ArrowRight || event == Event::Character('l')) { editor.move_right(); return true; }
            } else if (editor.mode == Mode::Insert) {
                if (event == Event::Escape) { editor.set_mode(Mode::Normal); return true; }
                if (event == Event::ArrowUp) { editor.move_up(); return true; }
                if (event == Event::ArrowDown) { editor.move_down(); return true; }
                if (event == Event::ArrowLeft) { editor.move_left(); return true; }
                if (event == Event::ArrowRight) { editor.move_right(); return true; }
                if (event == Event::Return) { editor.new_line(); return true; }
                if (event == Event::Backspace) { editor.backspace(); return true; }
                if (event.is_character()) { editor.insert_char(event.character()[0]); return true; }
            } else if (editor.mode == Mode::Command) {
                if (event == Event::Escape) { editor.set_mode(Mode::Normal); return true; }
                if (event == Event::Return) { editor.execute_command(); return true; }
                if (event == Event::Backspace) {
                    if (!editor.command_buffer.empty()) { editor.command_buffer.pop_back(); }
                    return true;
                }
                if (event.is_character()) { editor.command_buffer += event.character(); return true; }
            }
        } else {

            if (event == editor.key_bindings.quit.key) { editor.is_running = false; return true; }
            if (event == editor.key_bindings.save.key) { editor.save_file(); return true; }

            if (event == editor.key_bindings.move_up.key) { editor.move_up(); return true; }
            if (event == editor.key_bindings.move_down.key) { editor.move_down(); return true; }
            if (event == editor.key_bindings.move_left.key) { editor.move_left(); return true; }
            if (event == editor.key_bindings.move_right.key) { editor.move_right(); return true; }

            // Fallback for arrow keys
            if (event == Event::ArrowUp) { editor.move_up(); return true; }
            if (event == Event::ArrowDown) { editor.move_down(); return true; }
            if (event == Event::ArrowLeft) { editor.move_left(); return true; }
            if (event == Event::ArrowRight) { editor.move_right(); return true; }

            if (event == Event::Return) { editor.new_line(); return true; }
            if (event == Event::Backspace) { editor.backspace(); return true; }
            if (event.is_character()) { editor.insert_char(event.character()[0]); return true; }
        }

        return false;
    });

    screen.Loop(event_handler);
    return 0;
}
