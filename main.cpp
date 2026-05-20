#include "first_app.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(){
    std::cerr << "[DEBUG main] Starting FirstApp construction..." << std::endl << std::flush;

    try {
        lve::FirstApp app{};
        std::cerr << "[DEBUG main] FirstApp constructed successfully" << std::endl << std::flush;

        std::cerr << "[DEBUG main] Calling app.run()..." << std::endl << std::flush;
        app.run();
        std::cerr << "[DEBUG main] app.run() returned normally" << std::endl << std::flush;
    } catch(const std::exception &e){
        std::cerr << "[DEBUG main] Exception caught: " << e.what() << std::endl << std::flush;
        return EXIT_FAILURE;
    } catch(...){
        std::cerr << "[DEBUG main] Unknown exception caught!" << std::endl << std::flush;
        return EXIT_FAILURE;
    }

    std::cerr << "[DEBUG main] Exiting successfully" << std::endl << std::flush;
    return EXIT_SUCCESS;
}
