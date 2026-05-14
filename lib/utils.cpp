#include "utils.hpp"
#include <iostream>
#include <chrono>

void vc_timer(void) {
    static bool running = false;
    static std::chrono::steady_clock::time_point t0 =
        std::chrono::steady_clock::now();
    if (!running) {
        running = true;
    } else {
        std::chrono::duration<double> ts =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::steady_clock::now() - t0);
        std::cout << "Tempo decorrido: " << ts.count() << "s\n";
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}