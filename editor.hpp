#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "ftxui/component/event.hpp"


struct KeyBinding {
    ftxui::Event key;
    std::string description;
};


struct KeyBindingSet {
    KeyBinding save;
    KeyBinding quit;
    KeyBinding move_up;
    KeyBinding move_down;
    KeyBinding move_left;
    KeyBinding move_right;
};

enum class Mode {
    Normal,
    Insert,
    Command,
};

class Editor {
public:

    nlohmann::json config;
    KeyBindingSet key_bindings;
    std::vector<std::string> buffer;
    std::optional<std::string> filename;
    int cursor_x = 0;
    int cursor_y = 0;
    Mode mode = Mode::Normal;
    bool is_running = true;


    std::string command_buffer;
    std::string status_message;


    int scroll_offset_y = 0;
    int scroll_offset_x = 0;
    int terminal_height = 0;
    int terminal_width = 0;


    Editor(const std::string& file);
    void load_file(const std::string& file);
    void save_file();


    void load_config();
    void create_default_config(const std::filesystem::path& path);
    void set_keybindings(const std::string& preset_name);


    std::string mode_str() const;
    void set_mode(Mode new_mode);


    void execute_command();


    void move_left();
    void move_right();
    void move_up();
    void move_down();
    void insert_char(char c);
    void backspace();
    void new_line();

private:
    std::filesystem::path get_config_path();
    void clamp_cursor();
    void scroll_to_cursor();
};
