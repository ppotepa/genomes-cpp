#include <ThreeppInputAdapter.hpp>

#include <threepp/input/MouseListener.hpp>

#include <cassert>

namespace {

struct Listener final : threepp::MouseListener {
    int downs{0};
    int ups{0};
    int moves{0};
    int wheels{0};
    int last_button{-1};
    threepp::Vector2 last{};

    void onMouseDown(int button, const threepp::Vector2& pos) override {
        ++downs; last_button=button; last=pos;
    }
    void onMouseUp(int button, const threepp::Vector2& pos) override {
        ++ups; last_button=button; last=pos;
    }
    void onMouseMove(const threepp::Vector2& pos) override {
        ++moves; last=pos;
    }
    void onMouseWheel(const threepp::Vector2& delta) override {
        ++wheels; last=delta;
    }
};

} // namespace

int main() {
    genomes::render::ThreeppInputAdapter adapter;
    adapter.setViewport(500, 20, 700, 600);
    assert(adapter.size().width()==700);
    assert(adapter.size().height()==600);

    Listener listener;
    adapter.addMouseListener(listener);

    genomes::input::InputFrame press{};
    press.mouse_x=600.0F; press.mouse_y=100.0F;
    press.events.push_back({genomes::input::EventType::MouseButtonDown,0,0,1,
                            600.0F,100.0F,0.0F,0.0F,{}});
    adapter.feed(press);
    assert(listener.downs==1 && listener.last_button==0);
    assert(listener.last.x==100.0F && listener.last.y==80.0F);

    genomes::input::InputFrame drag{};
    drag.mouse_x=1300.0F; drag.mouse_y=700.0F;
    drag.events.push_back({genomes::input::EventType::MouseMove,0,0,0,
                           1300.0F,700.0F,25.0F,30.0F,{}});
    adapter.feed(drag);
    assert(listener.moves==1);
    assert(listener.last.x==800.0F && listener.last.y==680.0F);

    genomes::input::InputFrame release{};
    release.mouse_x=1300.0F; release.mouse_y=700.0F;
    release.events.push_back({genomes::input::EventType::MouseButtonUp,0,0,1,
                              1300.0F,700.0F,0.0F,0.0F,{}});
    adapter.feed(release);
    assert(listener.ups==1);

    genomes::input::InputFrame outside{};
    outside.mouse_x=100.0F; outside.mouse_y=100.0F;
    outside.events.push_back({genomes::input::EventType::MouseButtonDown,0,0,1,
                              100.0F,100.0F,0.0F,0.0F,{}});
    adapter.feed(outside);
    assert(listener.downs==1);

    genomes::input::InputFrame wheel{};
    wheel.mouse_x=700.0F; wheel.mouse_y=200.0F;
    wheel.events.push_back({genomes::input::EventType::MouseWheel,0,0,0,
                            0.0F,1.0F,0.0F,1.0F,{}});
    adapter.feed(wheel);
    assert(listener.wheels==1);
    assert(listener.last.y==1.0F);
    return 0;
}
