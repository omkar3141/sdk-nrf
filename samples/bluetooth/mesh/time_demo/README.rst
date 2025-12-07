.. _bluetooth_mesh_time_demo:

Bluetooth Mesh: Time Models Demo
#################################

.. contents::
   :local:
   :depth: 2

The Bluetooth Mesh Time Demo sample demonstrates the use of Time Server and Time Client models in a Bluetooth Mesh network.
It allows nodes to synchronize time information and take on different roles (Time Authority, Time Client, or Time Relay).

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires a smartphone with Nordic Semiconductor's nRF Mesh mobile app installed in one of the following versions:

* `nRF Mesh mobile app for Android`_
* `nRF Mesh mobile app for iOS`_

Overview
********

This sample demonstrates the Time Server and Time Client models from the Bluetooth Mesh Model specification.
The sample allows a device to function in three different time roles:

* **Time Authority** - Publishes time status messages but does not process received time status messages.
* **Time Client** - Processes received time status messages but does not publish them.
* **Time Relay** - Both processes and publishes time status messages.

The sample provides:

* Button controls to switch between time roles.
* Shell commands to configure time role and set the current time.
* Periodic logging of the current time every 5 seconds.
* LED indicators showing the current role.
* Default time initialization: On boot, if time is not already set (e.g., from persistent storage), the device automatically sets a default time of ``01-01-2025:00-00-00``.

Time Models
===========

The sample uses the following Bluetooth Mesh models:

Time Server
   The Time Server model is used to represent the current TAI (International Atomic Time) and local time on a mesh node.
   It maintains time zone offset and TAI-UTC delta information.

Time Client
   The Time Client model is used to interact with Time Server models.
   It can query and set time information on remote nodes.

Provisioning
============

The provisioning is handled by the :ref:`bt_mesh_dk_prov`.
It supports four types of out-of-band (OOB) authentication methods, and uses the Hardware Information driver to generate a deterministic UUID to uniquely represent the device.

Use `nRF Mesh mobile app`_ for provisioning and configuration of models supported by the sample.

Models
======

The following table shows the Bluetooth Mesh composition of this sample:

.. table::
   :align: center

   +------------------------------+
   | Element 0                    |
   +==============================+
   | Config Server                |
   +------------------------------+
   | Health Server                |
   +------------------------------+
   | Time Server                  |
   +------------------------------+
   +------------------------------+
   | Element 1                    |
   +==============================+
   | Time Client                  |
   +------------------------------+

The models are used as follows:

* **Config Server** allows configurator devices to configure the node remotely.
* **Health Server** provides ``attention`` callbacks that are used during provisioning to call your attention to the device.
  These callbacks trigger blinking of the LEDs.
* **Time Server** maintains the current time and responds to time queries.
  It also publishes time status messages based on the configured role.
* **Time Client** receives time status messages from other nodes and can query/set time on remote nodes.

User interface
**************

Buttons:
   Button 1:
      Set the device to **Time Authority** role.
      The device will publish time status messages.

   Button 2:
      Set the device to **Time Client** role.
      The device will only receive and process time status messages.

   Button 3:
      Set the device to **Time Relay** role.
      The device will both receive and publish time status messages.

   Button 4:
      Press and hold for 5 seconds to perform a factory reset.

LEDs:
   LED 1:
      Indicates the device is in **Time Authority** role.

   LED 2:
      Indicates the device is in **Time Client** role.

   LED 3:
      Indicates the device is in **Time Relay** role.

   LED 4:
      Indicates the device is provisioned.

Shell commands:
   The sample provides the following shell commands over UART:

   ``time_role set <role>``
      Set the time role. Valid values: 0 (Authority), 1 (Client), 2 (Relay).

   ``time_role get``
      Get the current time role.

   ``time set <DD-MM-YYYY:HH-MM-SS>``
      Set the current time on the Time Server.
      Example: ``time set 06-12-2025:14-30-00``

   ``time get``
      Get and display the current time from the Time Server.

Configuration
*************

|config|

Source file setup
=================

This sample is split into the following source files:

* :file:`main.c` - Application initialization and button handling.
* :file:`model_handler.c` - Time models implementation and shell commands.
* :file:`model_handler.h` - Model handler interface.

Building and running
********************

.. |sample path| replace:: :file:`samples/bluetooth/mesh/time_demo`

.. include:: /includes/build_and_run.txt

Testing
*******

After programming the sample to your development kit, you can test it by using a smartphone with `nRF Mesh mobile app`_ installed.
Testing consists of provisioning the device, configuring the models, and observing the time synchronization.

Provisioning the device
=======================

.. |device name| replace:: :guilabel:`Mesh Time Demo`

.. include:: /includes/mesh/mesh_dk_prov_testing.txt

Configuring models
==================

See :ref:`ug_bt_mesh_model_config_app` for details on how to configure the Bluetooth Mesh models with the nRF Mesh mobile app.

The configuration depends on which demonstration approach you choose:

* **Basic demonstration (Part 1)**: Uses only Time Server models for time synchronization.
  This is the recommended starting point.
* **Advanced demonstration (Part 2)**: Additionally uses Time Client model to trigger time updates remotely.

Basic configuration (Part 1) - Time Server only
************************************************

This configuration allows Time Servers to synchronize time using Time Status messages published to a group address.

1. In the nRF Mesh app, navigate to each node's configuration screen.

#. Expand the **Time Server** model (Element 0).

#. Tap :guilabel:`BIND KEY` and select an application key.

#. Configure publication:

   a. Tap :guilabel:`PUBLICATION` and configure it to publish to a group address (e.g., ``0xC000``).
   #. Set the publication period as desired (e.g., 60 seconds for periodic updates).

#. Configure subscription:

   a. Tap :guilabel:`SUBSCRIPTION` and add the same group address (e.g., ``0xC000``).

   .. note::
      Time Server models must subscribe to the group address to receive Time Status messages from other Time Servers.
      This enables time synchronization across all devices in the network.

#. Repeat steps 1-5 for all devices in your network.

Advanced configuration (Part 2) - Adding Time Client
*****************************************************

This optional configuration allows using the Time Client model to trigger time updates remotely by sending Time Set messages.

1. Complete the basic configuration (Part 1) first.

#. On the device where you want to use the Time Client model, expand the **Time Client** model (Element 1).

#. Tap :guilabel:`BIND KEY` and select the same application key.

#. Configure publication:

   a. Tap :guilabel:`PUBLICATION` and configure it to publish to the unicast address of the Time Authority device's Time Server.
      For example, if the Time Authority is at address ``0x0003``, publish to ``0x0003``.

   .. note::
      The Time Client model does not need a subscription configured for basic operation.
      Time Servers handle receiving Time Status messages.

Testing time synchronization
=============================

This section provides two demonstration approaches:

* **Part 1 (Basic)**: Demonstrates Time Server model synchronization using shell commands and periodic publication.
  Time Servers in different roles (Authority, Relay, Client) synchronize via group subscriptions.
* **Part 2 (Advanced)**: Demonstrates using the Time Client model to remotely set time on the Time Authority.

The demonstrations use three development kits to show how time propagates through the mesh network.

Prerequisites
*************

Before testing time synchronization, ensure you have:

* Three development kits programmed with this sample
* A serial terminal application (e.g., PuTTY, Tera Term, or minicom)
* The nRF Mesh mobile app installed on your smartphone
* All three devices provisioned and configured as described in previous sections

Step 1: Connect UART terminals
*******************************

Before starting the demonstration, connect to each device over UART to observe the logs and use shell commands.
This is the very first step to ensure you can monitor the time synchronization process.

1. Identify the COM port for each development kit:

   * On Windows, check Device Manager under "Ports (COM & LPT)"
   * On Linux, look for ``/dev/ttyACM*`` devices
   * On macOS, look for ``/dev/tty.usbmodem*`` devices

#. Open a serial terminal for each of the three devices with the following settings:

   * Baud rate: 115200
   * Data bits: 8
   * Parity: None
   * Stop bits: 1
   * Flow control: None

#. After connecting, you should see periodic log messages showing the initialization and current status.

#. Now we will assume the following:

   * Terminal 1: Will become the Time Client (Device 1)
   * Terminal 2: Will become the Time Relay (Device 2)
   * Terminal 3: Will become the Time Authority (Device 3)

Step 2: Set up the Time Client
*******************************

Set up the first device as a Time Client.
This device will only receive and synchronize time but will not relay it to other nodes.

1. On Device 1's UART terminal, press **Button 2** on the development kit to set it as a Time Client.

#. Verify that **LED 2** turns on, indicating the Time Client role.

#. Observe the log message:

   .. code-block:: console

      Role: Time Client

#. Every 5 seconds, you will see the current time logged.
   On first boot, the device starts with a default time of ``2025-01-01 00:00:00``:

   .. code-block:: console

      Current time: 2025-01-01 00:00:00
      Current time: 2025-01-01 00:00:05
      Current time: 2025-01-01 00:00:10

#. Leave this device running.
   The device will continue showing the default time until it receives a Time Status message from the network.

Step 3: Set up the Time Relay
******************************

Set up the second device as a Time Relay.
This device will receive time updates and relay them to other nodes in the network.

1. On Device 2's UART terminal, press **Button 3** on the development kit to set it as a Time Relay.

#. Verify that **LED 3** turns on, indicating the Time Relay role.

#. Observe the log message:

   .. code-block:: console

      Role: Time Relay

#. Similar to the Time Client, you will see periodic time logs showing the default time:

   .. code-block:: console

      Current time: 2025-01-01 00:00:00
      Current time: 2025-01-01 00:00:05

#. Leave this device running.
   Both Device 1 and Device 2 are now showing the default time and waiting for time synchronization from the network.

Part 1: Basic Time Server synchronization (using shell commands)
******************************************************************

This part demonstrates time synchronization using only Time Server models.
Time is set on the Time Authority using a shell command, and synchronization happens through Time Status messages published to the group address.

Step 4: Set up the Time Authority and observe propagation
==========================================================

Now set up the third device as the Time Authority, which will be the source of time in the network.
This step will demonstrate how time propagates through the mesh network.

1. On Device 3's UART terminal, press **Button 1** on the development kit to set it as a Time Authority.

#. Verify that **LED 1** turns on, indicating the Time Authority role.

#. Observe the log message:

   .. code-block:: console

      Role: Time Authority

#. Initially, this device will also show the default time:

   .. code-block:: console

      Current time: 2025-01-01 00:00:00
      Current time: 2025-01-01 00:00:05

#. Set the current time on the Time Authority using the shell command.
   Replace the date and time with the current values:

   .. code-block:: console

      uart:~$ time set 06-12-2025:14-30-00

   The format is ``DD-MM-YYYY:HH-MM-SS`` (24-hour format).

#. You should see a confirmation message on Device 3:

   .. code-block:: console

      Time set to: 2025-12-06 14:30:00

#. The Time Authority will immediately publish a Time Status message to the group address (``0xC000``).
   Device 3 will start logging the current time every 5 seconds:

   .. code-block:: console

      Current time: 2025-12-06 14:30:00
      Current time: 2025-12-06 14:30:05
      Current time: 2025-12-06 14:30:10

   .. note::
      The ``time set`` command triggers an immediate publication.
      Additionally, the Time Server will publish periodic Time Status messages based on the publication period configured in the nRF Mesh app (e.g., every 60 seconds).

Step 5: Observe time synchronization across the network
********************************************************

Now watch the time propagate from the Time Authority through the network.

1. On Device 2 (Time Relay), you will see it receive the Time Status message:

   .. code-block:: console

      Time Status received from 0x0003:
        TAI seconds: 1733493000
        TAI subseconds: 0/256
        Uncertainty: 0 ms
        TAI-UTC Delta: 37 s
        Time Zone Offset: 0 * 15 min
        Authority: yes
        Local time: 2025-12-06 14:30:00

#. The Time Relay will now start displaying the synchronized time:

   .. code-block:: console

      Current time: 2025-12-06 14:30:10
      Current time: 2025-12-06 14:30:15

#. On Device 1 (Time Client), you will see it receive the Time Status message.
   It may receive it directly from the Time Authority or relayed through the Time Relay:

   .. code-block:: console

      Time Status received from 0x0002:
        TAI seconds: 1733493010
        TAI subseconds: 0/256
        Uncertainty: 25 ms
        TAI-UTC Delta: 37 s
        Time Zone Offset: 0 * 15 min
        Authority: no
        Local time: 2025-12-06 14:30:10

   Note: The uncertainty has increased (typically 20-50 ms) because the message was relayed through Device 2.
   The ``Authority`` field shows ``no`` because Device 2 (Time Relay) is not the original authoritative source.

#. The Time Client will now display the synchronized time:

   .. code-block:: console

      Current time: 2025-12-06 14:30:10
      Current time: 2025-12-06 14:30:15

#. All three devices are now synchronized and displaying the same time!

#. Observe the ``Authority`` field in the Time Status messages:

   * Messages from Device 3 (Time Authority) show ``Authority: yes``
   * Messages relayed by Device 2 (Time Relay) show ``Authority: no``

#. This demonstrates the hierarchical time synchronization in the Bluetooth Mesh network.

Step 6: Verify synchronization
*******************************

To verify that all three devices are properly synchronized:

1. Compare the time displayed on all three UART terminals.
   They should all show the same time (within a second).

#. On Device 1 (Time Client), query the current time:

   .. code-block:: console

      uart:~$ time get

   Example output:

   .. code-block:: console

      Current time: 2025-12-06 14:30:45
      TAI seconds: 1733493045
      Uncertainty: 0 ms

#. On Device 2 (Time Relay), query the current time:

   .. code-block:: console

      uart:~$ time get

   The output should show the same time as Device 1.

#. The time will continue to advance on all devices, maintaining synchronization.

Step 7: Test shell commands
****************************

The sample provides shell commands for runtime control and inspection.

Query current time role:

.. code-block:: console

   uart:~$ time_role get

Example output:

.. code-block:: console

   Current time role: Authority

Change time role via shell (optional):

.. code-block:: console

   uart:~$ time_role set 1

This sets the device to Time Client role (0 = Authority, 1 = Client, 2 = Relay).

Update time on Time Authority:

.. code-block:: console

   uart:~$ time set 06-12-2025:15-00-00

This will update the time on the Time Authority and propagate it through the network.
Watch the terminals to see the updated time synchronize across all devices.

Step 8: Understand time synchronization behavior
*************************************************

The demonstration you just completed showcases the Bluetooth Mesh Time model's ad-hoc time synchronization hierarchy.
Here's what happened:

* **Time Authority** (Device 3): The single source of truth for time in the network.
  When you set the time using the ``time set`` command, it became the authoritative time source.
  The ``is_authority`` flag was set to ``yes``, indicating it has a reliable time source.
  It publishes Time Status messages but does not accept time updates from other nodes.

* **Time Relay** (Device 2): Received the Time Status from the Time Authority and synchronized its clock.
  It then relayed this time information to other nodes in the network, extending the coverage.
  The ``is_authority`` flag in messages from the Relay is ``no`` because it's not the original source.

* **Time Client** (Device 1): Received Time Status messages from neighboring nodes (either directly from the Authority or via the Relay).
  It synchronized its clock but does not relay the information further.
  This role is suitable for leaf nodes that only need to know the current time.

Key concepts demonstrated:

* **Default time initialization**: On boot, all devices start with a default time of ``01-01-2025:00-00-00`` if time is not already set from persistent storage.
  This ensures devices always have a valid time reference for demonstration purposes.

* **Message propagation**: You observed how time propagated from Device 3 → Device 2 → Device 1.
  Each device received a Time Status message and synchronized accordingly.

* **Uncertainty tracking**: Each time synchronization message includes uncertainty information.
  The Time Authority starts with 0 ms uncertainty (plus any mesh hop uncertainty from transmission).
  When a Time Relay forwards a message, it adds 20-50 ms of uncertainty to account for processing delays.
  Nodes only update their time if the received message has lower uncertainty than their current time.
  Uncertainty increases with each mesh hop and over time due to clock drift.

* **TAI Time**: Time is represented as TAI (International Atomic Time) seconds.
  The sample uses a TAI-UTC delta of 37 seconds (current as of 2025).
  This accounts for leap seconds added to UTC time.

* **One-hop transmission**: Unsolicited Time Status messages are transmitted one hop at a time.
  Each receiving node processes the message and, if it's a Time Relay, may relay it to its neighbors.
  This ensures proper uncertainty tracking and prevents flooding the network.

* **Role-based behavior**: The demonstration clearly showed how each role behaves differently:
  Authority publishes, Relay receives and relays, Client only receives.

Part 2: Advanced demonstration (using Time Client model)
*********************************************************

This part demonstrates using the Time Client model to remotely set time on a Time Authority device.
This requires completing the advanced configuration (Part 2) described in the Configuring models section.

Prerequisites for Part 2
=========================

* Complete Part 1 demonstration first
* Configure the Time Client model on one device to publish to the Time Authority's unicast address (e.g., ``0x0003``)
* Have all three devices still running from Part 1

Step 1: Identify the Time Client device
========================================

For this demonstration, we'll use Device 1 (currently in Time Client role) to send a Time Set message via its Time Client model instance.

1. Verify Device 1 is still in Time Client role (LED 2 should be on).

#. Ensure the Time Client model on Device 1 is configured to publish to the Time Authority's unicast address (configured in the nRF Mesh app).

Step 2: Trigger a remote time update using Time Client
=======================================================

Now we'll demonstrate how to remotely update the Time Authority's time using the Time Client model.

1. On Device 1's UART terminal, use the Time Client model to send a Time Set message.

   .. note::
      The current sample implementation uses the ``time set`` shell command which operates on the local Time Server.
      To fully demonstrate Part 2, you would need to extend the shell commands to include a ``time_client set`` command that sends a Time Set message via the Time Client model to the configured publication address.
      This would trigger the Time Setup Server on the Time Authority to update its time.

#. When a Time Set message is received by the Time Authority (Device 3):

   a. The Time Setup Server updates the local time.
   #. The Time Server immediately publishes a Time Status message to the group address.
   #. Other devices (Device 1 and Device 2) receive this Time Status message via their group subscription and synchronize their time.

Step 3: Observe the time propagation
=====================================

1. Watch Device 3 (Time Authority) update its time and publish the Time Status message.

#. Device 2 (Time Relay) receives the Time Status message and synchronizes.
   It then relays the message to its neighbors.

#. Device 1 (Time Client) receives the Time Status message and synchronizes.

#. All devices now show the updated time.

Key differences from Part 1
============================

* **Part 1** uses a shell command (``time set``) to directly update the local Time Server on the Time Authority.
  This is suitable for manual time configuration during initial setup or debugging.

* **Part 2** uses the Time Client model to send a Time Set message over the mesh network to remotely update the Time Authority.
  This demonstrates the client-server interaction defined in the Bluetooth Mesh Time model specification.
  This approach is more aligned with production scenarios where time might be set remotely from a gateway or provisioner application.

Both approaches result in the Time Authority publishing Time Status messages that synchronize the entire network.

Troubleshooting
***************

Time Client not receiving updates:

* Verify that the Time Server publication is configured correctly in the nRF Mesh app
* Ensure the Time Client subscription matches the Time Server publication address
* Check that both devices are using the same application key
* Verify that devices are within radio range (Time Status messages are sent one hop at a time)

Time not updating:

* Devices start with a default time of ``01-01-2025:00-00-00`` on boot if time is not already set
* Ensure you set the time on the Time Authority using the ``time set`` command
* Verify the Time Authority role is active (LED 1 should be on)
* Check that publication is configured with a suitable period (e.g., 10 seconds)

Shell commands not working:

* Ensure you're connected via UART with correct settings (115200 8N1)
* Verify the shell is enabled (it is by default in this sample)
* Press Tab to see available commands

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`bt_mesh_time_srv_readme`
* :ref:`bt_mesh_time_cli_readme`
* :ref:`bt_mesh_dk_prov`
* :ref:`dk_buttons_and_leds_readme`
