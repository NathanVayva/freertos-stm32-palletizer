# Palletizer — Driving a Factory I/O scene with FreeRTOS on STM32

Control of a palletizer simulated in **Factory I/O** (`Palletizer_1_4.factoryio`) by a **Nucleo STM32F072RB** board running **FreeRTOS**. The board reads the scene's sensors and drives its actuators over a serial link (USART2 + DMA) relayed by the Factory I/O Python bridge.

## Contents

```
Palletizer/
├── Palletizer_1_4.factoryio   # Factory I/O scene
└── TP3/                       # STM32CubeIDE project
    ├── app/        # main.c (tasks), FreeRTOSConfig.h, interrupts, printf
    ├── bsp/        # LED, button, USART2 console + DMA, Factory I/O frames
    ├── FreeRTOS/   # FreeRTOS kernel (8 KB heap, 1 ms tick)
    ├── TraceRecorder/  # Percepio Tracealyzer (J-Link RTT streaming)
    ├── cmsis/      # CMSIS + STM32F0 headers
    └── Debug/      # Build outputs (TP3.elf, .map, .list)
```

## Requirements

- Nucleo-F072RB board
- STM32CubeIDE
- Factory I/O + serial Python bridge (not included in this folder)
- Optional: Percepio Tracealyzer with a J-Link probe for tracing

## Usage

1. Open `TP3/` in STM32CubeIDE (*File → Import → Existing Projects into Workspace*).
2. Build and flash (`TP3 Debug.launch`).
3. Open `Palletizer_1_4.factoryio` in Factory I/O and start the simulation.
4. Run the Python bridge on the Nucleo's COM port (USART2, PA2/PA3, **115200 baud**, 8N1).

## Software architecture

The system is built on a **publish / subscribe** model:

- Business tasks **subscribe** to a sensor state by sending `{handle, sensor mask, expected state}` to `xSubscribeQueue`, then block on a task notification.
- `Task_Pub` (priority 2, 200 ms period) requests an update from Factory I/O, reads the sensors from `rx_dma_buffer`, and **notifies** every subscriber whose condition `(sensors & mask) == state` is true. Table holds at most 5 subscriptions.
- Tasks send their **commands** `{mask, value}` to `xCommandQueue`.
- `Task_Write` (priority 2) is the only UART writer: it keeps the global actuator state, builds the frame and sends it via DMA (channel 4), then waits for transfer completion (notification from the DMA IRQ). It also forwards text debug messages.
- Reception: DMA channel 5 in circular mode, so `rx_dma_buffer` is always up to date.

### Business tasks (priority 1)

| Task | Role |
|---|---|
| `Distribution_carton` | Starts the conveyors, dispenses a box, waits for it to pass (`Carton_Envoye`), then waits for the go-ahead from the blocker. |
| `bloqueur_poussoir` | Lets two boxes through at the entry, blocks, pushes the row; every 3 pushes, wakes `porte`. |
| `porte` | Clamps, stops the conveyor, waits for the elevator at level 2, opens the door (drops the layer), unclamps, waits for the descent, closes. |
| `ascenseur` | Loads a pallet, raises it to the door, lowers it layer by layer, then ejects the pallet and signals `distribution_palette`. |
| `distribution_palette` | Dispenses an empty pallet (4 s) then waits for the elevator's signal. |

Inter-task synchronization uses **indexed notifications**: index 0 = publisher, index 1 = task-to-task signal.

### Serial protocol

Actuator frame (7 bytes): `0xAD | d0 | d1 | d2 | d3 | CRC(0) | '\n'`
Received sensor frame (7 bytes): `0xA8 | d0 | d1 | d2 | d3 | CRC | '\n'`
Update request: `0xA3 '\n'`

Only 7 bits per byte are used (the MSB is reserved for tags), hence the gaps at bits 7, 15, 23 in the mapping in `app/inc/main.h`.

### Main mapping (`main.h`)

| Bit | Actuator | Sensor |
|---|---|---|
| 0 | Distribution_Cartons | Carton_Distribue |
| 1 | Tapis_Distribution_Cartons | Carton_Envoye |
| 2 | Blocage_Entree_Palettiseur | Entree_Palettiseur |
| 3 | Porte | Porte_Ouverte |
| 4 | Poussoir | Limite_Poussoir |
| 5 | Clamp | Clamped |
| 6 | Monter_Ascenseur | Ascenseur_Etage_RDC |
| 8 | Descendre_Ascenseur | Ascenseur_Etage_1 |
| 9 | Ascenseur_To_Limit | Ascenseur_Etage_2 |
| 10 | Distribution_Palette | Sortie_Palette |
| 11 | Charger_Palette | Limite_Porte |
| 12 | Tapis_Carton_vers_Palettiseur | Ascenseur_en_mouvement |
| 13 | Tourner_Carton | Entree_Palette |
| 14 | Decharger_Palettiseur | Butee_Carton |
| 16–21 | Charger_Palettiseur, Décharger_Palette, Tapis_Palette_Vers_Ascenseur, Tapis_Distribution_Palette, Tapis_Fin, Remover | — |

## Known limitations

- CRC not implemented (always `0x00`).
- Several delays are hand-tuned (e.g. `vTaskDelay(2270)` in `ascenseur`) and depend on the simulation speed.
- `SystemClock_Config()` is disabled: the MCU runs on the default clock (HSI 8 MHz), and the USART BRR is set accordingly.
- Queue sends use a 0 timeout: if `xCommandQueue` (12) or `xSubscribeQueue` (5) is full, the message is silently dropped.
