#include <iostream>

int main() {
    /*
     * MTL receive worker process entrypoint.
     *
     * This executable is the future owner of real MTL runtime state:
     *
     *   mtl_init()
     *   st20p_rx_create()
     *   st30p_rx_create()
     *   mtl_uninit() before process exit
     *
     * The OBS process must communicate with this worker through explicit IPC
     * and shared-memory media transport. OBS-process sink pointers must never
     * cross this process boundary.
     */
    std::cout << "st2110_mtl_rx_worker skeleton\n";
    return 0;
}