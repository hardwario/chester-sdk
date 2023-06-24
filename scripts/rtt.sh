#!/bin/sh

#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

set -eu

touch ~/rtt.log

tmux new-session \; \
  bind-key -n C-q kill-session \; \
  send-keys 'JLinkRTTLogger -Device NRF52840_XXAA -RTTChannel 1 -if SWD -Speed 4000 ~/rtt.log' C-m \; \
  split-window -h -p 70 \; \
  send-keys 'tail -f ~/rtt.log 2>/dev/null' C-m \; \
  select-pane -t 0 \; \
  split-window -v -p 70 \; \
  send-keys 'while true; do nc localhost 19021; sleep 0.5; done' C-m
