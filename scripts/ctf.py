# Copyright (c) Mathias Kaerlev 2013-2017.
#
# This file is part of cuwo.
#
# cuwo is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# cuwo is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with cuwo.  If not, see <http://www.gnu.org/licenses/>.

"""
Capture the flag gamemode

XXX not done yet
"""

import asyncio
from cuwo.script import ServerScript, ConnectionScript, command, admin
from cuwo.common import format_time
from cuwo import packet as packets
from cuwo.vector import vec3
from cuwo import constants
from cuwo import entity as entitydata


ROUND_START = 'CTF round started! Ending in {time}'
ROUND_END = '{name} won the round with {points} points!'
SCORE_POINT = '{name} scored a point!'
TAKE_FLAG = '{name} took a flag!'
DROP_FLAG = '{name} dropped the flag!'
NEXT_ROUND = 'Next round starting in {time}'
NOT_PLACING = 'No entity is being placed'


COLLISION_RADIUS = constants.BLOCK_SCALE * 2


class CTFConnection(ConnectionScript):
    place_entity = None
    flag = None
    points = 0

    def on_entity_update(self, event):
        entity = self.entity

        if not self.parent.playing or entity.hp <= 0:
            return

        mask = event.mask

        if self.place_entity and entitydata.is_ray_set(mask):
            pos = entity.get_ray_hit()
            pos.z += constants.BLOCK_SCALE // 4
            self.place_entity.set_position(pos)

        if not entitydata.is_pos_set(mask) and not entitydata.is_ray_set(mask):
            return

        if self.flag is not None:
            for base in self.parent.bases:
                if abs(base.pos - entity.pos) < COLLISION_RADIUS:
                    self.flag.set_position(self.flag.spawn_pos)
                    self.server.send_chat(SCORE_POINT.format(name=entity.name))
                    self.server.play_sound(self.parent.config.score_sound)
                    self.flag.holder = None
                    self.flag = None
                    break
            else:
                new_pos = entity.pos.copy()
                new_pos -= entity.get_look_dir() * constants.BLOCK_SCALE * 2
                self.flag.set_position(new_pos)
            return

        for flag in self.parent.flags:
            if flag.holder is not None or flag.placing:
                continue
            if abs(flag.pos - entity.pos) < COLLISION_RADIUS:
                self.server.send_chat(TAKE_FLAG.format(name=entity.name))
                self.server.play_sound(self.parent.config.take_sound)
                self.flag = flag
                flag.holder = self
                flag.set_position(entity.pos)
                break

    def on_die(self, event):
        self.remove_flag()

    def on_unload(self):
        self.remove_flag()

    def remove_flag(self):
        if not self.flag:
            return
        self.server.send_chat(DROP_FLAG.format(name=self.entity.name))
        self.server.play_sound(self.parent.config.drop_sound)
        self.flag.holder = None
        self.flag = None


def make_int_color(r, g, b, a):
    return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)


class CTFServer(ServerScript):
    connection_class = CTFConnection
    playing = False

    def on_load(self):
        self.config = self.server.config.ctf
        self.round_task = self.loop.create_task(self.run())
        self.particle_task = self.loop.create_task(self.run_particles())
        self.flags = []
        self.bases = []

    def create_flag(self):
        flag = self.world.create_entity()
        flag.holder = None
        flag.placing = True
        flag.set_type(None)
        flag.name = 'Flag'
        flag.hostile_type = constants.FRIENDLY_TYPE
        flag.appearance.flags = constants.IMMOVABLE_FLAG
        flag.appearance.set_body(self.config.flag_model)
        flag.appearance.scale.set(2.0, 2.0, 2.0)
        flag.appearance.body_scale = 0.7
        flag.appearance.body_offset.set(0.0, 0.0, 5)
        flag.reset()

        self.flags.append(flag)

        return flag

    def create_base(self):
        base = self.world.create_entity()
        base.set_type(None)
        base.name = 'Base'
        base.hostile_type = constants.FRIENDLY_TYPE
        base.appearance.flags = constants.IMMOVABLE_FLAG
        base.appearance.set_body(self.config.base_model)
        base.appearance.scale.set(2.0, 2.0, 2.0)
        base.appearance.body_scale = 0.7
        base.appearance.body_offset.set(0.0, 0.0, 2)
        base.reset()

        self.bases.append(base)

        return base

    @asyncio.coroutine
    def run(self):
        config = self.config
        server = self.server

        while True:
            # round start
            self.playing = True
            server.play_sound(config.start_sound)
            for entities in (self.flags, self.bases):
                for entity in entities:
                    entity.set_position(entity.spawn_pos)

            for player in self.children:
                player.points = 0

            message = ROUND_START.format(time=format_time(config.duration))
            print(message)
            server.send_chat(message)
            yield from asyncio.sleep(config.duration)

            # round end
            self.playing = False
            server.play_sound(config.end_sound)

            winner = None
            for player in self.children:
                if winner is None or winner.points < player.points:
                    winner = player

            if winner is None:
                name = 'nobody'
            else:
                name = winner.entity.name

            message = ROUND_END.format(name=name, points=winner.points)
            print(message)
            server.send_chat(message)

            yield from asyncio.sleep(1)

            # round restart
            message = NEXT_ROUND.format(time=format_time(config.restart_time))
            print(message)
            server.send_chat(message)
            yield from asyncio.sleep(config.restart_time)

    @asyncio.coroutine
    def run_particles(self):
        while True:
            for entities in (self.flags, self.bases):
                for entity in entities:
                    particle = packets.ParticleData()
                    particle.pos = entity.pos
                    particle.accel = vec3(0.0, 0.0, 8)
                    particle.color = make_int_color(250, 40, 99, 255)
                    particle.scale = 0.5
                    particle.count = 2
                    particle.particle_type = constants.NO_GRAVITY_PARTICLE
                    particle.spreading = 3
                    particle.something18 = 0
                    self.server.update_packet.particles.append(particle)

            yield from asyncio.sleep(0.2)

    def on_unload(self):
        self.round_task.cancel()
        self.particle_task.cancel()
        for entities in (self.flags, self.bases):
            for entity in entities:
                entity.destroy()

    def get_mode(self, event):
        return 'ctf'


def get_class():
    return CTFServer


@command
@admin
def addflag(script):
    """Add a CTF flag."""
    script.place_entity = script.parent.create_flag()


@command
@admin
def addbase(script):
    """Add a CTF base."""
    script.place_entity = script.parent.create_base()


@command
@admin
def put(script):
    """Freeze the CTF flag or base position."""
    entity = script.place_entity
    if not entity:
        return NOT_PLACING
    entity.placing = False
    entity.spawn_pos = script.place_entity.pos
    script.place_entity = None
