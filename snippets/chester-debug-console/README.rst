.. _snippet-chester-debug-console:

CHESTER Debug Console Snippet (chester-debug-console)
#####################################################

.. code-block:: console

   west build -S chester-debug-console [...]

Overview
********

This snippet redirects serial console output to UART1 and uses P0.12 (TAMPER_KEY)
as TX and P0.26 (BTN_EXT) as RX.
