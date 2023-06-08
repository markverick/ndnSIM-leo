import csv
import pandas as pd
def get_fast_update():
    next_hop = {}
    last_update = {}
    update_time_list = []
    change_counts = {}
    count_list = []
    for i in range(0, 5000):
        ns = int(i * 100000000)
        with open('scenarios/routes/starlink_unicast/fstate_{}.txt'.format(ns), newline='') as csvfile:
            rows = csv.reader(csvfile, delimiter=',')
            for row in rows:
                if (((int(row[0]), int(row[1])) in next_hop) and (next_hop[(int(row[0]), int(row[1]))] != int(row[2]))):
                    if (int(row[0]) == 1543 and int(row[1]) == 1612):
                        update_time_list.append((i - last_update[(int(row[0]), int(row[1]))], i, row[0], row[1], row[2]))
                next_hop[(int(row[0]), int(row[1]))] = int(row[2])
                if ((int(row[0]), int(row[1])) in change_counts):
                    change_counts[(int(row[0]), int(row[1]))] += 1
                else:
                    change_counts[(int(row[0]), int(row[1]))] = 1
                last_update[(int(row[0]), int(row[1]))] = i;
    # print(sorted(update_time_list))
    for p, c in change_counts.items():
        count_list.append((c, p))
    return (sorted(update_time_list), sorted(count_list, reverse=True))
# Beijing to New York, until t=211s
gfu = get_fast_update()
print(gfu[0][0:10])
print(gfu[1][0:10])

# get_route(1593, 1590, 211)
