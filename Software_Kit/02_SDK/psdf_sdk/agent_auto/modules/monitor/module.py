#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AgentAuto monitor module: capture shell or HIF UART output."""

COMMAND = "monitor"


def add_parser(sub, engine) -> None:
    engine.add_monitor_parser(sub)
