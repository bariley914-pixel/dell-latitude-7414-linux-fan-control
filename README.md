# Dell Latitude 7414 Rugged Extreme fan control on Linux

Verified on one Latitude 7414 with BIOS 1.39.0. The stock Linux Dell fan controls could read the tachometer but failed to maintain a manual override. Direct Dell SMM Method 1 reached about 4,900 RPM at full speed and accepted the restore command. Medium measured about 2,500 RPM in testing. The app offers Automatic and Full Speed.

## Build and test

On Mint or Ubuntu, install gcc and the GTK 3 Python bindings. Build the included helpers as a regular user:

```sh
sudo apt install build-essential python3-gi gir1.2-gtk-3.0
gcc -O2 -Wall -Wextra -Werror probe.c -o probe
gcc -O2 -Wall -Wextra -Werror override-test.c -o override-test
sudo ./probe
sudo ./override-test
```

The probe checks both Dell signatures and reads the fan slots without changing speed. On the tested machine it found slot 0. The second command checks that the restore command is accepted, requests full speed for eight seconds, and sends the restore command again. Start with the timed test. Don't scan random SMM commands or use this helper on an unverified model. If a separate fan service such as i8kmon is active, it may compete with the override.

## Desktop app

Only after the timed test works, install the helper to a root-owned location and the app to your account:

```sh
sudo install -o root -g root -m 0755 override-test /usr/local/sbin/dell-fan-override
install -D -m 0755 dell-fan-control "$HOME/.local/bin/dell-fan-control"
sudo visudo -f /etc/sudoers.d/99-dell-fan-control
```

Add the following line using your own Linux login in place of YOUR_LOGIN. These are the only two commands the app needs.

```sudoers
YOUR_LOGIN ALL=(root) NOPASSWD: /usr/local/sbin/dell-fan-override --automatic, /usr/local/sbin/dell-fan-override --hold-full
```

Run `"$HOME/.local/bin/dell-fan-control"`. Full Speed holds the override without a timer. Choose Automatic to hand control back to the firmware. Closing the app leaves the selected speed in effect. If the app is unavailable, run `sudo /usr/local/sbin/dell-fan-override --automatic`.

## Firmware details

- Signatures: try 0xffa3 and fall back to 0xfea3. The first signature alone did not work on the tested unit.
- Restore automatic: 0x31a3. Enter manual override: 0x30a3.
- Set speed: 0x01a3, with EBX 0x0200 for full speed on slot 0. Read tach: 0x02a3, with EBX 0 for slot 0.
- References: [dellfan Method 1](https://github.com/clopez/dellfan), [Linux Dell SMM driver documentation](https://docs.kernel.org/hwmon/dell-smm-hwmon.html).
