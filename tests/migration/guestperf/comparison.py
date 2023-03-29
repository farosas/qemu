#
# Migration test scenario comparison mapping
#
# Copyright (c) 2016 Red Hat, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#

from guestperf.scenario import Scenario

class Comparison(object):
    def __init__(self, name, scenarios):
        self._name = name
        self._scenarios = scenarios

COMPARISONS = [
    # Looking at effect of fixed-ram + multifd with varying numbers of
    # channels
    Comparison("fixed-ram", scenarios = [
        Scenario("fixed-ram-multifd-channels-2",
                 multifd=True, multifd_channels=2,
                 fixed_ram=True, bandwidth=0),
        Scenario("fixed-ram-multifd-channels-4",
                 multifd=True, multifd_channels=4,
                 fixed_ram=True, bandwidth=0),
        Scenario("fixed-ram-multifd-channels-8",
                 multifd=True, multifd_channels=8,
                 fixed_ram=True, bandwidth=0),
        Scenario("fixed-ram-multifd-channels-16",
                 multifd=True, multifd_channels=16,
                 fixed_ram=True, bandwidth=0),
        Scenario("fixed-ram-multifd-channels-32",
                 multifd=True, multifd_channels=32,
                 fixed_ram=True, bandwidth=0),
        Scenario("fixed-ram-multifd-channels-64",
                 multifd=True, multifd_channels=64,
                 fixed_ram=True, bandwidth=0),
        Scenario("fixed-ram-multifd-channels-128",
                 multifd=True, multifd_channels=128,
                 fixed_ram=True, bandwidth=0)
    ]),

    Comparison("dio-fixed-ram", scenarios = [
        Scenario("dio-fixed-ram-multifd-channels-2",
                 multifd=True, multifd_channels=2,
                 fixed_ram=True, bandwidth=0, direct_io=True),
        Scenario("dio-fixed-ram-multifd-channels-4",
                 multifd=True, multifd_channels=4,
                 fixed_ram=True, bandwidth=0, direct_io=True),
        Scenario("dio-fixed-ram-multifd-channels-8",
                 multifd=True, multifd_channels=8,
                 fixed_ram=True, bandwidth=0, direct_io=True),
        Scenario("dio-fixed-ram-multifd-channels-16",
                 multifd=True, multifd_channels=16,
                 fixed_ram=True, bandwidth=0, direct_io=True),
        Scenario("dio-fixed-ram-multifd-channels-32",
                 multifd=True, multifd_channels=32,
                 fixed_ram=True, bandwidth=0, direct_io=True),
        Scenario("dio-fixed-ram-multifd-channels-64",
                 multifd=True, multifd_channels=64,
                 fixed_ram=True, bandwidth=0, direct_io=True),
        Scenario("dio-fixed-ram-multifd-channels-128",
                 multifd=True, multifd_channels=128,
                 fixed_ram=True, bandwidth=0, direct_io=True)
    ]),
]
