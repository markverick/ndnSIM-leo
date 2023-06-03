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

import exputil

try:
    from .run_list import *
except (ImportError, SystemError):
    from run_list import *

local_shell = exputil.LocalShell()

local_shell.remove_force_recursive("runs")
local_shell.remove_force_recursive("pdf")
local_shell.remove_force_recursive("data")

# NDN runs
for run in get_ndn_run_list():

    # Prepare run directory
    run_dir = "runs/" + run["name"]
    local_shell.remove_force_recursive(run_dir)
    local_shell.make_full_dir(run_dir)

    # config_ns3.properties
    local_shell.copy_file("templates/template_ndn_a_b_config_ns3.properties", run_dir + "/config_ns3.properties")
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[SATELLITE-NETWORK]", str(run["satellite_network"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[DYNAMIC-STATE]", str(run["dynamic_state"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[DYNAMIC-STATE-UPDATE-INTERVAL-NS]", str(run["dynamic_state_update_interval_ns"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[SIMULATION-END-TIME-NS]", str(run["simulation_end_time_ns"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[ISL-DATA-RATE-MEGABIT-PER-S]", str(run["isl_data_rate_megabit_per_s"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[GSL-DATA-RATE-MEGABIT-PER-S]", str(run["gsl_data_rate_megabit_per_s"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[ISL-MAX-QUEUE-SIZE-PKTS]", str(run["isl_queue_size_pkt"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[GSL-MAX-QUEUE-SIZE-PKTS]", str(run["gsl_queue_size_pkt"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[ISL-ERROR-RATE]", str(run["isl_error_rate"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[GSL-ERROR-RATE]", str(run["gsl_error_rate"]))
    local_shell.sed_replace_in_file_plain(run_dir + "/config_ns3.properties",
                                          "[NDN-CLIENT]", str(run["ndn_client"]))


# Print finish
print("Success: generated runs")
