.. _bluetooth_mesh_light_switch_light:

Bluetooth Mesh: Switch Light demo
##################################

This sample runs the **same firmware** on two nRF54L15 DKs: each device has one
Generic OnOff **Client** (acting as a switch, bound to Button 1) and one
Generic OnOff **Server** (acting as a light, driving LED 1). After provisioning
and configuring publication/subscription with the nRF Mesh app, pressing the
button on one DK can control the LED on the other DK.

Requirements
************

* Two nRF54L15 development kits
* nRF Mesh mobile app (Android or iOS) for provisioning and configuration

Hardware mapping
****************

* **Button 1 (SW0)** – OnOff Client: press to send OnOff Set (toggle)
* **LED 1 (LED0)** – OnOff Server: reflects OnOff state; also used for
  attention during provisioning

Shell (UART)
************

UART shell is enabled with the mesh command set. After provisioning, run
``mesh init`` once, then you can use OnOff client commands, for example:

* ``mesh onoff get <addr>`` – get OnOff state from a node
* ``mesh onoff set <addr> <0|1>`` – set OnOff state on a node

Use ``mesh help`` and ``mesh onoff help`` for more options.

Building and flashing
*********************

From the sample directory, with SDK environment active (e.g. ``source sdk-v302.sh``):

.. code-block:: bash

   west build -p -b nrf54l15dk/nrf54l15/cpuapp
   west flash

Repeat the build and flash on both DKs (same firmware).

Evaluating the demo (two DKs)
*****************************

1. **Provision both DKs** with the nRF Mesh app (add both as nodes, provision
   each; note their unicast addresses, e.g. 0x0001 and 0x0002).

2. **Configure the “switch” DK (e.g. DK1, 0x0001):**
   * Open the node → Element 0 → Generic OnOff Client.
   * Set **Publish** to the unicast address of the “light” DK (e.g. 0x0002),
     or to a group address if you prefer.

3. **Configure the “light” DK (e.g. DK2, 0x0002):**
   * Open the node → Element 0 → Generic OnOff Server.
   * Set **Subscribe** to the same address you used as Publish on the switch
     (e.g. 0x0001 or a group). If you use unicast publish from DK1, the server
     on DK2 typically subscribes to a group and DK1 publishes to that group so
     that DK2 receives the messages.

   Simpler option: have DK1’s OnOff Client **publish** to DK2’s **unicast**
   address (e.g. 0x0002). Then DK2’s server receives messages sent to that
   unicast address by default (no subscription needed for unicast destination).

4. **Test:**
   * On DK1, press Button 1. The LED on **DK2** (the light) should toggle.
   * You can repeat the configuration the other way (DK2 as switch, DK1 as
     light) so that each DK can control the other’s LED.

Models
******

| Element | Models |
|---------|--------|
| 0       | Config Server, Health Server, Generic OnOff Client, Generic OnOff Server |

Publication and subscription are configured at runtime via the nRF Mesh app;
no compile-time pub/sub.
