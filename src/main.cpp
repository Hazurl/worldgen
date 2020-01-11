#include <SFML/Graphics.hpp>

#include <PerlinNoise.hpp>

#include <iostream>
#include <random>
#include <cmath>
#include <cassert>
#include <functional>

#include <wgen/Dimension.hpp>

using Filter = std::function<wgen::Cell(wgen::Cell, int, int)>;

template<typename T, typename...Ts>
Filter compose(T&& t, Ts&&... ts) {
    if constexpr (sizeof...(Ts) <= 0) {
        return std::forward<T>(t);
    } else {
        return [t, f = compose(ts...)] (wgen::Cell cell, int x, int y) {
            return f(t(cell, x, y), x, y);
        };
    }
}

double clamp (double v, double a, double b) {
    if (v < a) { return a; }
    if (v > b) { return b; }
    return v;
}

sf::Color height_color_map(wgen::Cell cell) {
    return sf::Color { cell.height * 255, cell.height * 255, cell.height * 255 };
}

sf::Color humidity_color_map(wgen::Cell cell) {
    return sf::Color { 0, cell.humidity * 255, 0 };
}

sf::Color biome_color_map(wgen::Cell cell) {
    if (cell.height < 0.3) {
        return sf::Color{ 0, 51, 153 }; // deep water
    }

    if (cell.height < 0.4) {
        return sf::Color{ 0, 153, 255 }; // shallow water
    }

    if (cell.height > 0.9) {
        return sf::Color{ 240, 240, 240 }; // polar
    }

    if (cell.height > 0.8) {
        return sf::Color{ 0, 51, 0 }; // polar
    }

    if (cell.height > 0.7) {
        if (cell.humidity > 0.4) {
            return sf::Color{ 0, 102, 102 };
        } else {
            return sf::Color{ 116, 139, 123 };
        }
    }

    if (cell.height > 0.5) {
        if (cell.humidity > 0.5) {
            return sf::Color{ 0, 153, 153 };
        } else if (cell.humidity > 0.3) {
            return sf::Color{ 51, 153, 102 };
        } else {
            return sf::Color{ 190, 194, 131 };
        }
    }

    if (cell.humidity < 0.3) {
        return sf::Color{ 227, 231, 107 };
    } else if (cell.humidity < 0.7) {
        return sf::Color{ 0, 179, 63 };
    } else {
        return sf::Color{ 0, 153, 0 };
    }
    return sf::Color{ 255, 0, 255 };
}

int main() {

    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 800;

    using Dim = wgen::Dimension<wgen::Cell>;
    std::unique_ptr<Dim> dim = std::make_unique<Dim>();

    sf::Texture texture;
    if(!texture.create(Dim::width, Dim::height)) {
        std::cerr << "Couldn't create the texture.\n";
        return 1;
    }

    sf::Sprite sprite(texture);

    sf::RenderWindow window (sf::VideoMode(WIDTH, HEIGHT), "[SFML] World Generation", sf::Style::Default);

    {
        auto desktop = sf::VideoMode::getDesktopMode();
        window.setPosition({ static_cast<int>(desktop.width - window.getSize().x) / 2, static_cast<int>(desktop.height - window.getSize().y) / 2 });
    }

    sf::Color clear_color{ 255, 0, 0 };

    wgen::Mode mode = wgen::Mode::All;
    using color_map_t = sf::Color(*)(wgen::Cell);
    color_map_t color_map = biome_color_map;

    srand(time(nullptr));

    std::uint32_t seed = rand();

    struct Cell {
        int height;
    };

    Filter init_filter_perlin{ [&seed] (wgen::Cell, int x, int y) -> wgen::Cell {
        static std::uint32_t last_seed = seed;
        static siv::PerlinNoise p;
        if (last_seed != seed) {
            last_seed = seed;
            p.reseed(seed);
        }

        return wgen::Cell {
            p.octaveNoise0_1((static_cast<double>(x) + 78943) / (WIDTH / 3.), (static_cast<double>(y) + 94567) / (HEIGHT / 3.), 4),
            p.octaveNoise0_1(static_cast<double>(x) / (WIDTH / 10.), static_cast<double>(y) / (HEIGHT / 10.), 3),
        };
    }};

    Filter init_0_filter{ [] (wgen::Cell, int, int) -> wgen::Cell {
        return wgen::Cell{ 0, 0 };
    }};

    Filter init_1_filter{ [] (wgen::Cell, int, int) -> wgen::Cell {
        return wgen::Cell{ 1, 1 };
    }};

    Filter round_island_filter{ [] (wgen::Cell c, int x, int y) -> wgen::Cell {
        double dx = (x - static_cast<double>(WIDTH) / 2.) / (static_cast<double>(WIDTH) / 2.);
        double dy = (y - static_cast<double>(HEIGHT) / 2.) / (static_cast<double>(HEIGHT) / 2.);
        double dist = std::sqrt(dx * dx + dy * dy);

        dist = clamp(dist, 0, 1);
        dist *= dist * dist * dist;

        dist = 1 - dist;

        c.height *= dist;
        return c;
    }};

    Filter square_island_filter{ [] (wgen::Cell c, int x, int y) -> wgen::Cell {
        double dx = (x - static_cast<double>(WIDTH) / 2.) / (static_cast<double>(WIDTH) / 2.);
        if (dx < 0) dx = -dx;

        double dy = (y - static_cast<double>(HEIGHT) / 2.) / (static_cast<double>(HEIGHT) / 2.);
        if (dy < 0) dy = -dy;

        double dist = dx > dy ? dx : dy;

        dist = clamp(dist, 0, 1);
        dist *= dist * dist * dist;

        dist = 1 - dist;

        c.height *= dist;
        return c;
    }};

    Filter round_perlin = compose(init_filter_perlin, square_island_filter, square_island_filter);

    auto perlin = [&seed] (double x, double y, int octave, double freq) -> double {
        static std::uint32_t last_seed = seed;
        static siv::PerlinNoise p;
        if (last_seed != seed) {
            last_seed = seed;
            p.reseed(seed);
        }

        return p.octaveNoise0_1(x / freq, y / freq, octave);
    };

    auto generate = [perlin, &mode] (int x, int y) -> wgen::Cell {
        static constexpr int width = 800;
        static constexpr int height = 800;

        double value = perlin((static_cast<double>(x) + 78943) / width, (static_cast<double>(y) + 94567) / height, 4, 1/3.);
        value = clamp(value, 0, 1);

        double dist;
        {
            double dx = static_cast<double>(x - static_cast<double>(width) / 2.) / (static_cast<double>(width) / 2.);
            double dy = static_cast<double>(y - static_cast<double>(height) / 2.) / (static_cast<double>(height) / 2.);
            dist = std::sqrt(dx * dx + dy * dy);

            dist = clamp(dist, 0, 1);
            dist *= dist * dist * dist;

            dist = 1 - dist;
        }

        double humidity = perlin(static_cast<double>(x) / width, static_cast<double>(y) / height, 3, 0.1);

        switch(mode) {
            case wgen::Mode::Height: return { value, humidity };
            case wgen::Mode::Round: return { dist, humidity };
            case wgen::Mode::Humidity: return { value, humidity };
            case wgen::Mode::All: return { dist * value, humidity };
        }
        assert(false);        
    };

    dim->generate_from_filter(round_perlin);
    dim->update_texture(texture, color_map);

    while(window.isOpen()) {
        sf::Event event;
        while(window.pollEvent(event)) {
            switch(event.type) {
                case sf::Event::Closed: {
                    window.close();
                    break;
                }
                case sf::Event::KeyPressed: {
                    if (event.key.code == sf::Keyboard::M) {
                        switch(mode) {
                            case wgen::Mode::Height:
                                std::cout << "Round\n";
                                mode = wgen::Mode::Round;
                                color_map = height_color_map;
                                break;
                            case wgen::Mode::Round:
                                std::cout << "Humidity\n";
                                mode = wgen::Mode::Humidity;
                                color_map = humidity_color_map;
                                break;
                            case wgen::Mode::Humidity:
                                std::cout << "All\n";
                                mode = wgen::Mode::All;
                                color_map = biome_color_map;
                                break;
                            case wgen::Mode::All:
                                std::cout << "Height\n";
                                mode = wgen::Mode::Height;
                                color_map = height_color_map;
                                break;
                        }

                        dim->generate_from_filter(round_perlin);
                        dim->update_texture(texture, color_map);
                    } else if (event.key.code == sf::Keyboard::Space) {
                        seed = rand();
                        dim->generate_from_filter(round_perlin);
                        dim->update_texture(texture, color_map);
                    }
                    break;
                }
                default: { break; }
            }
        }


        window.clear(clear_color);

        window.draw(sprite);

        window.display();
    }

}