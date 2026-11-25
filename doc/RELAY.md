# Relay

One relay channel, switched from the console and by Party mode. Library: [`lib/Relay/Relay.h`](../lib/Relay/Relay.h) (header-only).

## Wiring (1-channel 5 V module)

| Module pin | Connect to |
|------------|-----------|
| `DC+` / VCC | **5 V** rail of the buck converter (not 3.3 V) |
| `DC-` / GND | common GND |
| `IN` | **D7** |

Load side (screw terminals):

| Terminal | Connect to |
|----------|-----------|
| `COM` | supply side of the live / + wire |
| `NO` (normally open) | wire to the load, so it only gets power when the relay is on |
| `NC` | not used (use it instead of `NO` for "on by default") |

The load's return (GND / neutral) bypasses the relay.

**230 V warning.** For mains (Swiss plug: brown = live, blue = neutral, yellow-green = earth) only the brown wire goes through the relay. Never build it on a breadboard or with open terminals: use an insulated enclosure with strain relief and a fuse in the live wire, and have it checked by an experienced person before plugging it in. A ready-made switchable socket is the safer option.

## In the enclosure

In the final build the relay sits in the separate mains compartment (see [ENCLOSURE.md](ENCLOSURE.md)). Mains comes in through a detachable cord (Swiss plug to C13) into an IEC C14 panel inlet with a fuse drawer. The relay's `COM` takes the live wire from the inlet and `NO` goes to the Type 13 outlet for the load (the disco ball). Blue and yellow-green go straight through. Wiring diagram: [wiring v3](wiring/README.md).

## Active-low

Most 5 V relay modules (opto-coupler type) switch **on when `IN` is LOW**. `Relay` assumes this by default. If your relay does the opposite of what you ask, switch the polarity in `main.cpp`:

```cpp
Relay relay1(D7);          // active-low (default)
Relay relay1(D7, false);   // active-high
```

The relay is off after `relay1.begin()`; the pin gets the "off" level before it becomes an output, so it does not click at boot.

## Library

```cpp
#include <Relay.h>

Relay relay1(D7);

void setup() { relay1.begin(); }

relay1.on();
relay1.off();
relay1.toggle();
relay1.set(true);
relay1.isOn();      // the state we last asked for (the pin is never read back)
```

## Console

```
relay            show the state
relay on         switch on
relay off        switch off
relay toggle     flip it
```

`status` also shows the relay line. Works from any mode (see [SHELL.md](SHELL.md)).

## Settings

Settings mode has a **relay** entry (last in the list): SP toggles it between
*on* (Party may switch the relay) and *off* (the relay stays off; `relay on` is
ignored too). Switching it off turns a running relay off at once. The choice is
saved to flash, so it survives a power cycle. Console: `settings` shows the
selected setting, `settings -n` / `-p` navigate and toggle it.

In code: `relay1.setEnabled(false)` / `relay1.isEnabled()`.

## Modes

- **Party:** SP starts the party and switches the relay **on** (e.g. the spinning disco light); SP again, or leaving Party mode, switches it **off**.
- Other modes do not touch the relay. A mode that does not switch the relay off itself leaves it as it was, so use `relay off` if needed.

## Tests

`pio test -e native -f test_relay` (polarity, off after begin, on/off/toggle, enable/disable).
