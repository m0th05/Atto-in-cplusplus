#include "editor.hpp"
#include <fstream>
#include <iostream>
#include <algorithm> // for std::min/max
#include <cstdlib> // for getenv

using json = nlohmann::json;
using namespace ftxui;



Editor::Editor(const std::string &file) {
    load_config();

    set_keybindings(config.value("key_binding_preset", "atto"));

    if (config.value("vim_mode", true) == false) {
        mode = Mode::Insert;
    }
    load_file(file);
}


std::filesystem::path Editor::get_config_path() {

    std::filesystem::path local_path = "config.json";
    if (std::filesystem::exists(local_path)) {
        return local_path;
    }


    std::filesystem::path config_dir;

    #if defined(_WIN32)

        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            config_dir = std::filesystem::path(appdata) / "atto";
        }
    #elif defined(__APPLE__)

        const char* home = std::getenv("HOME");
        if (home) {
            config_dir = std::filesystem::path(home) / "Library" / "Application Support" / "atto";
        }
    #else

        const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
        if (xdg_config_home) {
            config_dir = std::filesystem::path(xdg_config_home) / "atto";
        } else {
            const char* home = std::getenv("HOME");
            if (home) {
                config_dir = std::filesystem::path(home) / ".config" / "atto";
            }
        }
    #endif


    if (!config_dir.empty()) {
        return config_dir / "config.json";
    }


    return local_path;
}

void Editor::load_config() {
    std::filesystem::path config_path = get_config_path();
    std::ifstream f(config_path);

    if (!f.good()) {

        if (config_path.has_parent_path()) {
            std::filesystem::create_directories(config_path.parent_path());
        }

        create_default_config(config_path);
        status_message = "Created default config at: " + config_path.string();
        f.open(config_path); // Re-open the file so we can parse it.
    }

    try {
        config = json::parse(f);
    } catch (json::parse_error& e) {
        status_message = "Error parsing config: " + std::string(e.what());

        config = {
            {"vim_mode", true},
            {"command_style", "vim"},
            {"key_binding_preset", "atto"}
        };
    }
}

void Editor::create_default_config(const std::filesystem::path& path) {
    std::ofstream o(path);
    json j = {
        {"vim_mode", true},
        {"command_style", "vim"},
        {"key_binding_preset", "atto"}
    };
    o << std::setw(4) << j << std::endl;
}

void Editor::set_keybindings(const std::string& preset_name) {
    if (preset_name == "nano") {
        key_bindings = {
            {Event::Character(char(15)), "Save"},   // Ctrl+O
            {Event::Character(char(24)), "Quit"},   // Ctrl+X
            {Event::ArrowUp, "Move Up"},
            {Event::ArrowDown, "Move Down"},
            {Event::ArrowLeft, "Move Left"},
            {Event::ArrowRight, "Move Right"}
        };
    } else if (preset_name == "micro") {
        key_bindings = {
            {Event::Character(char(19)), "Save"},   // Ctrl+S
            {Event::Character(char(17)), "Quit"},   // Ctrl+Q
            {Event::ArrowUp, "Move Up"},
            {Event::ArrowDown, "Move Down"},
            {Event::ArrowLeft, "Move Left"},
            {Event::ArrowRight, "Move Right"}
        };
    } else if (preset_name == "emacs") {
        key_bindings = {
            {Event::Character(char(24)), "Save"},   // Ctrl+X (simplified)
            {Event::Character(char(3)), "Quit"},    // Ctrl+C (simplified)
            {Event::Character(char(16)), "Move Up"},    // Ctrl+P
            {Event::Character(char(14)), "Move Down"},  // Ctrl+N
            {Event::Character(char(2)), "Move Left"},   // Ctrl+B
            {Event::Character(char(6)), "Move Right"}   // Ctrl+F
        };
    } else { // Default to "atto"
        key_bindings = {
            {Event::Character(char(19)), "Save"},   // Ctrl+S
            {Event::Character(char(17)), "Quit"},   // Ctrl+Q
            {Event::ArrowUp, "Move Up"},
            {Event::ArrowDown, "Move Down"},
            {Event::ArrowLeft, "Move Left"},
            {Event::ArrowRight, "Move Right"}
        };
    }
}



void Editor::load_file(const std::string &file) {
    filename = file;
    std::ifstream in(file);

    if (!in) {
        buffer.push_back("");
        if (status_message.empty()) {
            status_message = "\"" + file + "\" [New File]";
        }
        return;
    }

    buffer.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        buffer.push_back(line);
    }

    if (buffer.empty()) {
        buffer.push_back("");
    }
    if (status_message.empty()) {
        status_message = "\"" + filename.value_or("Untitled") + "\" " + std::to_string(buffer.size()) + "L read";
    }
}

void Editor::save_file() {
    if (!filename.has_value()) {
        status_message = "Error: No filename specified.";
        return;
    }

    std::ofstream out(filename.value());
    if (!out) {
        status_message = "Error: Could not open file for writing!";
        return;
    }

    for (size_t i = 0; i < buffer.size(); ++i) {
        out << buffer[i] << (i == buffer.size() - 1 ? "" : "\n");
    }
    status_message = "\"" + filename.value() + "\" " + std::to_string(buffer.size()) + "L written";
}

std::string Editor::mode_str() const {
    if (config.value("vim_mode", true) == false) {
        return "INSERT";
    }
    switch(mode) {
        case Mode::Normal: return "NORMAL";
        case Mode::Insert: return "INSERT";
        case Mode::Command: return "COMMAND";
        default: return "";
    }
}

void Editor::set_mode(Mode new_mode) {
    mode = new_mode;
    status_message.clear();
}

void Editor::execute_command() {
    std::string cmd = command_buffer;
    std::string style = config.value("command_style", "vim");

    bool quit = (style == "vim" && (cmd == "q" || cmd == "quit")) || (style == "kakoune" && cmd == "quit");
    bool write = (style == "vim" && cmd == "w") || (style == "kakoune" && cmd == "write");
    bool write_quit = (style == "vim" && cmd == "wq") || (style == "kakoune" && cmd == "write-quit");

    if (write_quit) {
        save_file();
        is_running = false;
    } else if (quit) {
        is_running = false;
    } else if (write) {
        save_file();
    } else {
        status_message = "Unknown command: " + cmd;
    }
    command_buffer.clear();
    set_mode(Mode::Normal);
}

void Editor::scroll_to_cursor() {
    if (cursor_y < scroll_offset_y) {
        scroll_offset_y = cursor_y;
    }
    if (cursor_y >= scroll_offset_y + terminal_height) {
        scroll_offset_y = cursor_y - terminal_height + 1;
    }
    if (cursor_x < scroll_offset_x) {
        scroll_offset_x = cursor_x;
    }
    if (cursor_x >= scroll_offset_x + terminal_width) {
        scroll_offset_x = cursor_x - terminal_width + 1;
    }
}

void Editor::clamp_cursor() {
    cursor_y = std::max(0, std::min((int)buffer.size() - 1, cursor_y));
    int line_len = buffer[cursor_y].length();
    cursor_x = std::max(0, std::min(line_len, cursor_x));
}

void Editor::move_up() {
    if (cursor_y > 0) cursor_y--;
    clamp_cursor();
    scroll_to_cursor();
}

void Editor::move_down() {
    if (cursor_y < buffer.size() - 1) cursor_y++;
    clamp_cursor();
    scroll_to_cursor();
}

void Editor::move_left() {
    if (cursor_x > 0) {
        cursor_x--;
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = buffer[cursor_y].length();
    }
    scroll_to_cursor();
}

void Editor::move_right() {
    if (cursor_y < buffer.size() && cursor_x < buffer[cursor_y].length()) {
        cursor_x++;
    } else if (cursor_y < buffer.size() - 1) {
        cursor_y++;
        cursor_x = 0;
    }
    scroll_to_cursor();
}

void Editor::insert_char(char c) {
    buffer[cursor_y].insert(cursor_x, 1, c);
    cursor_x++;
    scroll_to_cursor();
}

void Editor::new_line() {
    std::string rest_of_line = buffer[cursor_y].substr(cursor_x);
    buffer[cursor_y].erase(cursor_x);
    buffer.insert(buffer.begin() + cursor_y + 1, rest_of_line);
    cursor_y++;
    cursor_x = 0;
    scroll_to_cursor();
}

void Editor::backspace() {
    if (cursor_x > 0) {
        cursor_x--;
        buffer[cursor_y].erase(cursor_x, 1);
    } else if (cursor_y > 0) {
        std::string current_line = buffer[cursor_y];
        buffer.erase(buffer.begin() + cursor_y);
        cursor_y--;
        cursor_x = buffer[cursor_y].length();
        buffer[cursor_y] += current_line;
    }
    scroll_to_cursor();
}
