#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AgentAuto burn module: flash firmware and reset/start by default."""

COMMAND = "burn"


def add_parser(sub, engine) -> None:
    engine.add_burn_parser(sub)
