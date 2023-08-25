# The MIT License (MIT)
#
# Copyright (c) 2020 ETH Zurich
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Core values
dynamic_state_update_interval_ms = 100                          # 100 millisecond update interval
simulation_end_time_s = 200                                     # 200 seconds
pingmesh_interval_ns = 1 * 1000 * 1000                          # A ping every 1ms
enable_isl_utilization_tracking = True                          # Enable utilization tracking
isl_utilization_tracking_interval_ns = 1 * 1000 * 1000 * 1000   # 1 second utilization intervals

# Derivatives
dynamic_state_update_interval_ns = dynamic_state_update_interval_ms * 1000 * 1000
simulation_end_time_ns = simulation_end_time_s * 1000 * 1000 * 1000
dynamic_state = "dynamic_state_" + str(dynamic_state_update_interval_ms) + "ms_for_" + str(simulation_end_time_s) + "s"

# Error parameters
isl_error_rate = 0
gsl_error_rate = 0

# dynamic state algorithm
# paired: each ground station is connected to exactly one satellite
# free: each ground station can connect to any satellite that yields the shortest path

ds_alg = {}
ds_alg['paired'] = "starlink_550_isls_plus_grid_ground_stations_4_different_orbits_algorithm_paired_many_only_over_isls"
ds_alg['free'] = "starlink_550_isls_plus_grid_ground_stations_4_different_orbits_fast_algorithm_free_one_only_over_isls"

# dynamic state sub folder in scenarios/data/<dynamic_state>/<ds>
ds = "dynamic_state_100ms_for_200s"

# congestion control scheme list, WIP, currently unused
cc_list = ["TcpNewReno", "TcpCubic", "TcpVegas","TcpBbr"]

# NDN client list. Additional variables such as frequency and coneumers/producers
# parameters can be adjusted in the corresponding file
# Ping - experiments/scenarios/runs/a_b_ping.cc: one client constantly sends
#        interest <Frequency> packets per second with no retransmission timer
#        Segment number is unique, and is incrementing by one per interest transmission.
#        Use vanilla best-route forwarding strategy
#        and a modified `ConsumerPing' application (disabled retransmission)
# PingInstantRetx - experiments/scenarios/runs/a_b_ping_instant_retx.cc:
#                   same as Ping but the client will do instant retransmission
#                   on every pending interest when it detects a satellite handover.
#                   Use vanilla best-route forwarding strategy and a modified
#                   `ConsumerPing' application (disabled retransmission timer)
# PingNackRetx - experiments/scenarios/runs/a_b_ping_nack_retx.cc:
#                one client sends Frequency interest per second with no
#                retransmission timer. The cleint will do instant retransmission after it detects a satellite handover. Use custom nack-retx forwarding strategy and modified `ConsumerPing' application (disable retransmission timer). The satellites in the network will react to NACK to find the alternative path.
# FixedWindow - experiments/scenarios/runs/a_b_fixed_window.cc:
#               one client sends interests, allowing fixed-window pending interests. Use default rtt estimator for retransmission timer. Use custom ConsumerFixedWindow client
# FixedWindowRetx - experiments/scenarios/runs/a_b_fixed_window_retx.cc:
#                   just like Fixed window but with nack-retx strategy.
ndn_clients = ["Ping"]

# consumer vs producer pair
pairs = [
    (1584 + 0, 1584 + 1, 'Sao-Paulo 11000k'),
    # (1584 + 2, 1584 + 3, 'San-Jose 11000k'),
    # (1584 + 4, 1584 + 5, 'Montreal 11000k'),
    # (1584 + 6, 1584 + 7, 'Victoria 11000k'),
    # (1584 + 0, 1584 + 8, 'Sao-Paulo 5500k'),
    # (1584 + 2, 1584 + 9, 'San-Jose 5500k'),
    # (1584 + 4, 1584 + 10, 'Montreal 5500k'),
    # (1584 + 6, 1584 + 11, 'Victoria 5500k'),
]

# loss ratios for burst error model
ratios = []
# ratios = [
        # '0',
        # '3e-8',
        # '1e-7',
        # '3e-7',
        # '1e-6',
        # '3e-6',
        # '1e-5',
        # '3e-5',
        # '1e-4',
        # ]

# generate pairs of experiments
chosen_pairs = []
for nc in ndn_clients:
    for p in pairs:
        if (len(ratios) == 0):
            chosen_pairs.append(("starlink_550_isls", p[0], p[1], nc, "paired", 0, 0, p[2]))
        else:
            for r in ratios:
                chosen_pairs.append(("starlink_550_isls", p[0], p[1], nc, "paired", float(r), float(r), p[2]))


def get_ndn_run_list():
    run_list = []
    for p in chosen_pairs:
        run_list += [
            {
                "name": p[0] + "_" + str(p[1]) + "_to_" + str(p[2]) + "_with_" + p[3] + "_" + p[4] + "_loss_" + str(p[5]) + "_" + str(p[6]) + "_at_1_Gbps",
                "satellite_network": ds_alg[p[4]],
                "dynamic_state": ds,
                "dynamic_state_update_interval_ns": dynamic_state_update_interval_ns,
                "simulation_end_time_ns": simulation_end_time_ns,
                "isl_data_rate_megabit_per_s": 10000.0,
                "isl_queue_size_pkt": 100000,
                "gsl_data_rate_megabit_per_s": 10000.0,
                "gsl_queue_size_pkt": 100000,
                "enable_isl_utilization_tracking": enable_isl_utilization_tracking,
                "isl_utilization_tracking_interval_ns": isl_utilization_tracking_interval_ns,
                "from_id": p[1],
                "to_id": p[2],
                "ndn_client": p[3],
                "isl_error_rate": p[5],
                "gsl_error_rate": p[6],
                
            },
        ]

    return run_list
