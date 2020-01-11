#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <cassert>
#include <thread>
#include <iostream>
#include <functional>

namespace wgen {

struct Cell {
    double height;
    double humidity;
    //double temperature;
};

enum class Mode {
    Height,
    Round, 
    Humidity, 
    All
};

template<typename T>
class Dimension {
public:

    static constexpr unsigned width = 800;
    static constexpr unsigned height = 800;

    T& operator[](std::pair<std::size_t, std::size_t> const& p) {
        return data.at(p.first + p.second * width); 
    }

    T const& operator[](std::pair<std::size_t, std::size_t> const& p) const {
        return data.at(p.first + p.second * width); 
    }

    template<typename F>
    void generate(F&& f) {
        std::vector<std::thread> threads;

        const std::size_t dx = width / 8;
        const std::size_t dy = height / 8;

        for(std::size_t y{ 0 }; y < height; y += dy) {
            for(std::size_t x{ 0 }; x < width; x += dx) {
                //std::cout << "(" << x << ", " << y << ") => (" << x + dx << ", " << y + dy << ")\n";
                threads.emplace_back([f, x, y, dx, dy, &data = this->data] () {                    
                    for(std::size_t m{ y }; m < y + dy; ++m) {
                        for(std::size_t n{ x }; n < x + dx; ++n) {
                            data[m * width + n] = f(n, m);
                        }
                    }
                });
            }
        }

        for(auto& t : threads) {
            t.join();
        }
    }

    template<typename F>
    void generate_from_filter(F&& f) {
        std::vector<std::thread> threads;

        const std::size_t dx = width / 8;
        const std::size_t dy = height / 8;

        for(std::size_t y{ 0 }; y < height; y += dy) {
            for(std::size_t x{ 0 }; x < width; x += dx) {
                //std::cout << "(" << x << ", " << y << ") => (" << x + dx << ", " << y + dy << ")\n";
                threads.emplace_back([f, x, y, dx, dy, &data = this->data] () {                    
                    for(std::size_t m{ y }; m < y + dy; ++m) {
                        for(std::size_t n{ x }; n < x + dx; ++n) {
                            data[m * width + n] = f(Cell{ 0, 0 }, n, m);
                        }
                    }
                });
            }
        }

        for(auto& t : threads) {
            t.join();
        }
    }

    template<typename F>
    void update_texture(sf::Texture& texture, F&& color_of) const {
        std::array<sf::Uint8, width * height * 4> pixels;
        std::size_t p{ 0 };

        for(std::size_t y{ 0 }; y < Dimension<float>::height; ++y) {
            for(std::size_t x{ 0 }; x < Dimension<float>::width; ++x) {
                auto color = color_of((*this)[{x, y}]);
                pixels[p++] = color.r;
                pixels[p++] = color.g;
                pixels[p++] = color.b;
                pixels[p++] = color.a;
            }
        }

        texture.update(pixels.data());
    }

private:

    std::array<T, width * height> data;

};
/*
template<>
void Dimension<float>::update_texture(sf::Texture& texture, Mode mode) const {
    std::array<sf::Uint8, width * height * 4> pixels;
    std::size_t p{ 0 };

    for(std::size_t y{ 0 }; y < Dimension<float>::height; ++y) {
        for(std::size_t x{ 0 }; x < Dimension<float>::width; ++x) {
            auto value = (*this)[{x, y}];
            pixels.at(p++) = value * 255;
            pixels.at(p++) = value * 255;
            pixels.at(p++) = value * 255;
            pixels.at(p++) = 255;
        }
    }

    texture.update(pixels.data());
}
*/
/*
template<>
void Dimension<Cell>::update_texture(sf::Texture& texture, Mode mode) const {
    std::array<sf::Uint8, width * height * 4> pixels;
    std::size_t p{ 0 };

    std::function<sf::Color(Cell)> color_of = [] (Cell ccell) {
        auto exact_color = [] (Cell cell) {
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
    
        };

        double shift = 0.00;

        sf::Color n = exact_color({ ccell.height + shift, ccell.humidity });
        sf::Color s = exact_color({ ccell.height - shift, ccell.humidity });
        sf::Color e = exact_color({ ccell.height, ccell.humidity + shift });
        sf::Color w = exact_color({ ccell.height, ccell.humidity - shift });

        sf::Color ne = exact_color({ ccell.height + shift, ccell.humidity + shift });
        sf::Color nw = exact_color({ ccell.height + shift, ccell.humidity - shift });
        sf::Color se = exact_color({ ccell.height - shift, ccell.humidity + shift });
        sf::Color sw = exact_color({ ccell.height - shift, ccell.humidity - shift });

        sf::Color c = exact_color({ ccell.height, ccell.humidity });

        return sf::Color {
            ((n.r + w.r + e.r + s.r + ne.r + nw.r + se.r + sw.r) * 0.5 + c.r) / 5.,
            ((n.g + w.g + e.g + s.g + ne.g + nw.g + se.g + sw.g) * 0.5 + c.g) / 5.,
            ((n.b + w.b + e.b + s.b + ne.b + nw.b + se.b + sw.b) * 0.5 + c.b) / 5.
        };
    };

    if (mode == Mode::Height) {
        color_of = [] (Cell cell) {
            return sf::Color{ cell.height * 255, cell.height * 255, cell.height * 255 };
        };
    }

    else if (mode == Mode::Humidity) {
        color_of = [] (Cell cell) {
            return sf::Color{ cell.humidity * 255, cell.humidity * 255, cell.humidity * 255 };
        };
    }

    else if (mode == Mode::Round) {
        color_of = [] (Cell cell) {
            return sf::Color{ cell.height * 255, cell.height * 255, cell.height * 255 };
        };
    }

    for(std::size_t y{ 0 }; y < Dimension<float>::height; ++y) {
        for(std::size_t x{ 0 }; x < Dimension<float>::width; ++x) {
            auto color = color_of((*this)[{x, y}]);
            pixels[p++] = color.r;
            pixels[p++] = color.g;
            pixels[p++] = color.b;
            pixels[p++] = color.a;
        }
    }

    texture.update(pixels.data());
}
*/
}