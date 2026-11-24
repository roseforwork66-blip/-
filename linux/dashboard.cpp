#include "demo_scenarios.h"

#include <gtk/gtk.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

using cane::State;
using cane::Stop;
using cane::demo::Frame;
using cane::demo::Scenario;

struct Scene {
    Scenario scenario;
    const char* title;
    const char* description;
};

constexpr std::array<Scene, 5> scenes{{
    {Scenario::Straight, "前方通畅", "前方路径畅通，控制程序给出直行指令。"},
    {Scenario::Left, "左侧可绕", "正前方有障碍，控制程序选择从左侧通过。"},
    {Scenario::Right, "右侧可绕", "正前方有障碍，控制程序选择从右侧通过。"},
    {Scenario::Blocked, "前方阻塞", "候选路线受阻，程序关闭舵机输出并提示停步。"},
    {Scenario::ScanLoss, "雷达超时", "模拟扫描数据超时，程序关闭舵机输出并提示停步。"},
}};

const char* stateText(State state) {
    switch (state) {
        case State::Idle: return "待命";
        case State::Straight: return "直行";
        case State::Sidestep: return "侧向绕行";
        case State::Pass: return "通过障碍";
        case State::Stopped: return "提示停步";
    }
    return "状态未知";
}

const char* stopText(Stop stop) {
    switch (stop) {
        case Stop::None: return "—";
        case Stop::Obstacle: return "检测到障碍";
        case Stop::ScanStale: return "雷达数据超时";
        default: return "控制保护已触发";
    }
}

void addClass(GtkWidget* widget, const char* name) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), name);
}

GtkWidget* text(const char* value, const char* style) {
    GtkWidget* widget = gtk_label_new(value);
    gtk_label_set_xalign(GTK_LABEL(widget), 0);
    if (style) {
        addClass(widget, style);
    }
    return widget;
}

void pack(GtkWidget* box, GtkWidget* child, bool grow = false) {
    gtk_box_pack_start(GTK_BOX(box), child, grow, grow, 0);
}

void styleApp() {
    constexpr char style[] =
        "window.dashboard { background-color: #0b1720; color: #eaf4f5; }"
        ".dashboard label { color: #eaf4f5; }"
        ".dashboard label.caption { color: #a9bec7; }"
        ".dashboard label.badge { color: #79e8c4; font-weight: bold; }"
        ".dashboard label.title { font-size: 26px; font-weight: bold; }"
        ".dashboard label.heading { font-size: 16px; font-weight: bold; }"
        ".dashboard label.state { color: #79e8c4; font-size: 26px; font-weight: bold; }"
        ".dashboard label.halt { color: #ffba79; }"
        ".dashboard box.card { background-color: #132631; border: 1px solid #203b47;"
        " border-radius: 14px; padding: 16px; }"
        ".dashboard button { min-height: 30px; }"
        ".dashboard button.replay { background-image: none; background-color: #79e8c4;"
        " color: #0b1720; font-weight: bold; }"
        ".dashboard button.replay:hover { background-color: #a5f5d9; }";
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, style, -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void drawRoute(cairo_t* cr, double width, double height, Scenario scenario, State state) {
    cairo_set_source_rgb(cr, 0.04, 0.10, 0.15);
    cairo_paint(cr);

    const auto move = [=](double x, double y) { cairo_move_to(cr, width * x, height * y); };
    const auto line = [=](double x, double y) { cairo_line_to(cr, width * x, height * y); };
    const auto obstacle = [=](double left, double right, double top, double bottom) {
        cairo_rectangle(cr, width * left, height * top, width * (right - left), height * (bottom - top));
        cairo_set_source_rgba(cr, 0.98, 0.62, 0.35, 0.40);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.98, 0.68, 0.41, 0.85);
        cairo_stroke(cr);
    };

    cairo_set_source_rgb(cr, 0.15, 0.25, 0.31);
    cairo_set_line_width(cr, 1);
    for (int i = 1; i < 10; ++i) {
        const double fraction = i / 10.0;
        move(fraction, 0); line(fraction, 1);
        move(0, fraction); line(1, fraction);
    }
    cairo_stroke(cr);

    const bool detour = scenario == Scenario::Left || scenario == Scenario::Right;
    const double side = scenario == Scenario::Left ? 0.27 : 0.73;
    if (detour) {
        obstacle(0.43, 0.57, 0.37, 0.49);
        if (scenario == Scenario::Left) {
            obstacle(0.64, 0.93, 0.54, 0.62);
        } else {
            obstacle(0.07, 0.36, 0.54, 0.62);
        }
    } else if (scenario == Scenario::Blocked) {
        obstacle(0.13, 0.87, 0.40, 0.52);
    }

    const bool stopped = state == State::Stopped;
    const double red = stopped ? 1.0 : 0.47;
    const double green = stopped ? 0.73 : 0.91;
    const double blue = stopped ? 0.47 : 0.77;
    cairo_set_source_rgba(cr, red, green, blue, state == State::Idle ? 0.53 : 1.0);
    const double dash[]{7, 6};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_set_line_width(cr, 2.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    move(0.50, 0.82);
    if (detour) {
        cairo_curve_to(cr, 0.50 * width, 0.70 * height, side * width, 0.64 * height,
                       side * width, 0.51 * height);
        line(side, 0.16);
    } else {
        line(0.50, scenario == Scenario::Blocked ? 0.64 : 0.16);
    }
    cairo_stroke(cr);

    double x = 0.50;
    double y = 0.82;
    switch (state) {
        case State::Idle: break;
        case State::Straight: x = detour ? side : 0.50; y = 0.20; break;
        case State::Sidestep: x = side; y = 0.54; break;
        case State::Pass: x = side; y = 0.34; break;
        case State::Stopped: y = 0.66; break;
    }
    const auto dot = [=](double radius, double opacity) {
        cairo_arc(cr, width * x, height * y, radius, 0, 6.2831853);
        cairo_set_source_rgba(cr, red, green, blue, opacity);
        cairo_fill(cr);
    };
    dot(11, 0.24);
    dot(5, 1);
}

struct Dashboard {
    GtkWidget* window{};
    GtkWidget* selector{};
    GtkWidget* play{};
    GtkWidget* next{};
    GtkWidget* reset{};
    GtkWidget* map{};
    GtkWidget* state{};
    GtkWidget* metrics{};
    GtkWidget* caption{};
    std::vector<Frame> frames;
    std::size_t scene = 1;
    std::size_t index = 0;
    guint timer = 0;

    bool atEnd() const { return index + 1 == frames.size(); }

    void pause() {
        if (timer != 0) {
            g_source_remove(timer);
            timer = 0;
        }
    }

    void refresh() const {
        const Frame& frame = frames[index];
        const cane::Output& output = frame.output;
        gtk_label_set_text(GTK_LABEL(state), stateText(output.state));
        GtkStyleContext* stateStyle = gtk_widget_get_style_context(state);
        if (output.state == State::Stopped) {
            gtk_style_context_add_class(stateStyle, "halt");
        } else {
            gtk_style_context_remove_class(stateStyle, "halt");
        }
        gchar* values = g_strdup_printf("仿真时间   %u ms\n帧序号      %u / %u\n舵机输出   %s\n目标角度   %.2f°\n停步原因   %s",
                                        frame.milliseconds, static_cast<unsigned>(index + 1),
                                        static_cast<unsigned>(frames.size()), output.steeringEnabled ? "开启" : "关闭",
                                        static_cast<double>(output.steeringDeg), stopText(output.stop));
        gtk_label_set_text(GTK_LABEL(metrics), values);
        g_free(values);
        gtk_label_set_text(GTK_LABEL(caption), scenes[scene].description);
        gtk_button_set_label(GTK_BUTTON(play), timer ? "暂停" : "播放");
        gtk_widget_set_sensitive(next, !atEnd());
        gtk_widget_queue_draw(map);
    }

    void startOrPause() {
        if (timer) {
            pause();
        } else {
            if (atEnd()) {
                index = 0;
            }
            timer = g_timeout_add(950, +[](gpointer data) -> gboolean {
                auto& app = *static_cast<Dashboard*>(data);
                ++app.index;
                const bool done = app.atEnd();
                if (done) {
                    app.timer = 0;
                }
                app.refresh();
                return done ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
            }, this);
        }
        refresh();
    }

    void create() {
        styleApp();
        gtk_window_set_default_icon_name("smart-guide-cane");
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        addClass(window, "dashboard");
        gtk_window_set_title(GTK_WINDOW(window), "双轮式智能导盲杖 · Linux 演示");
        gtk_window_set_default_size(GTK_WINDOW(window), 840, 590);
        gtk_widget_set_size_request(window, 620, 480);
        gtk_container_set_border_width(GTK_CONTAINER(window), 24);

        GtkWidget* layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
        gtk_container_add(GTK_CONTAINER(window), layout);
        GtkWidget* header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        pack(header, text("导盲杖原型 / 离线仿真", "badge"));
        pack(header, text("双轮式智能导盲杖", "title"));
        pack(header, text("回放五个测试场景，查看模拟转向指令和停步原因。", "caption"));
        pack(layout, header);

        GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
        GtkWidget* selectorLabel = text("选择场景", nullptr);
        selector = gtk_combo_box_text_new();
        gtk_widget_set_size_request(selector, 130, -1);
        gtk_label_set_mnemonic_widget(GTK_LABEL(selectorLabel), selector);
        pack(controls, selectorLabel);
        pack(controls, selector);
        for (const Scene& option : scenes) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(selector), option.title);
        }
        pack(controls, gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), true);
        play = gtk_button_new_with_label("播放");
        next = gtk_button_new_with_label("下一帧");
        reset = gtk_button_new_with_label("重置");
        addClass(play, "replay");
        for (GtkWidget* button : {play, next, reset}) {
            gtk_widget_set_size_request(button, 75, -1);
            pack(controls, button);
        }
        pack(layout, controls);

        GtkWidget* main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
        GtkWidget* mapCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        addClass(mapCard, "card");
        pack(mapCard, text("路线示意", "heading"));
        map = gtk_drawing_area_new();
        gtk_widget_set_size_request(map, 260, 235);
        pack(mapCard, map, true);
        pack(main, mapCard, true);

        GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        addClass(info, "card");
        gtk_widget_set_size_request(info, 216, -1);
        pack(info, text("控制程序输出", "heading"));
        state = text("", "state");
        metrics = text("", nullptr);
        gtk_label_set_lines(GTK_LABEL(metrics), 5);
        pack(info, state);
        pack(info, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        pack(info, metrics);
        pack(main, info);
        pack(layout, main, true);

        caption = text("", "caption");
        gtk_label_set_line_wrap(GTK_LABEL(caption), TRUE);
        pack(layout, caption);

        g_signal_connect(window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
            static_cast<Dashboard*>(data)->pause();
            if (gtk_main_level()) {
                gtk_main_quit();
            }
        }), this);
        g_signal_connect(selector, "changed", G_CALLBACK(+[](GtkComboBox* dropdown, gpointer data) {
            auto& app = *static_cast<Dashboard*>(data);
            const int selected = gtk_combo_box_get_active(dropdown);
            if (selected >= 0 && static_cast<std::size_t>(selected) < scenes.size()) {
                app.pause();
                app.scene = static_cast<std::size_t>(selected);
                app.index = 0;
                app.frames = cane::demo::run(scenes[app.scene].scenario);
                app.refresh();
            }
        }), this);
        g_signal_connect(play, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            static_cast<Dashboard*>(data)->startOrPause();
        }), this);
        g_signal_connect(next, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto& app = *static_cast<Dashboard*>(data);
            app.pause();
            if (!app.atEnd()) {
                ++app.index;
            }
            app.refresh();
        }), this);
        g_signal_connect(reset, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto& app = *static_cast<Dashboard*>(data);
            app.pause();
            app.index = 0;
            app.refresh();
        }), this);
        g_signal_connect(map, "draw", G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer data) -> gboolean {
            const auto& app = *static_cast<Dashboard*>(data);
            drawRoute(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget),
                      scenes[app.scene].scenario, app.frames[app.index].output.state);
            return FALSE;
        }), this);

        gtk_combo_box_set_active(GTK_COMBO_BOX(selector), static_cast<int>(scene));
        gtk_widget_show_all(window);
    }
};

bool smokeTest(const Dashboard& app) {
    constexpr std::array<std::size_t, 5> counts{{2, 4, 4, 2, 3}};
    constexpr std::array<Stop, 5> reasons{{Stop::None, Stop::None, Stop::None, Stop::Obstacle, Stop::ScanStale}};
    for (std::size_t i = 0; i < scenes.size(); ++i) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.selector), static_cast<int>(i));
        if (app.scene != i || app.frames.size() != counts[i] || app.frames.front().output.state != State::Idle ||
            app.frames.back().output.stop != reasons[i] || app.frames.back().output.steeringEnabled != (i < 3)) {
            return false;
        }
        if ((i == 1 && app.frames[1].output.steeringDeg <= 0) || (i == 2 && app.frames[1].output.steeringDeg >= 0)) {
            return false;
        }
        gtk_button_clicked(GTK_BUTTON(app.next));
        if (app.index != 1) {
            return false;
        }
        gtk_button_clicked(GTK_BUTTON(app.reset));
        if (app.index != 0 || std::string_view(gtk_label_get_text(GTK_LABEL(app.state))) != "待命") {
            return false;
        }
    }
    gtk_button_clicked(GTK_BUTTON(app.play));
    const gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (app.timer != 0 && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    return app.timer == 0 && app.atEnd() && !gtk_widget_get_sensitive(app.next) &&
           std::string_view(gtk_label_get_text(GTK_LABEL(app.state))) == "提示停步";
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    if (!gtk_init_check(nullptr, nullptr)) {
        g_printerr("Smart Guide Cane requires an active Linux desktop session.\n");
        return 1;
    }
    Dashboard app;
    app.create();
    if (smoke) {
        const bool passed = smokeTest(app);
        gtk_widget_destroy(app.window);
        if (!passed) {
            g_printerr("Linux dashboard playback check failed.\n");
        }
        return passed ? 0 : 1;
    }
    gtk_main();
}
