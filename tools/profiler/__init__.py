# Please do not edit this file
import json
from collections import OrderedDict

import numpy as np
import matplotlib.pyplot as plt


DRLM_EVENTS = {
    # interested event name
    'BottomMLP.fc1': {
        'same_name_events_occured_order_in_code_forward': 0,
        'same_name_events_occured_order_in_code_backward': 6,
    },
    'BottomMLP.fc2': {
        'same_name_events_occured_order_in_code_forward': 1,
        'same_name_events_occured_order_in_code_backward': 5,
    },
    'BottomMLP.fc3': {
        'same_name_events_occured_order_in_code_forward': 2,
        'same_name_events_occured_order_in_code_backward': 4,
    },
    'sparse_embedding1': {
        'same_name_events_occured_order_in_code_forward': 0,
        'same_name_events_occured_order_in_code_backward': 0,
    },
    'interaction1': {
        'same_name_events_occured_order_in_code_forward': 0,
        'same_name_events_occured_order_in_code_backward': 0,
    },
    'TopMLP.fc4': {
        'same_name_events_occured_order_in_code_forward': 3,
        'same_name_events_occured_order_in_code_backward': 3,
    },
    'TopMLP.fc5': {
        'same_name_events_occured_order_in_code_forward': 4,
        'same_name_events_occured_order_in_code_backward': 2,
    },
    'TopMLP.fc6': {
        'same_name_events_occured_order_in_code_forward': 5,
        'same_name_events_occured_order_in_code_backward': 1,
    },
    'TopMLP.fc7': {
        'same_name_events_occured_order_in_code_forward': 6,
        'same_name_events_occured_order_in_code_backward': 0,
    },
    'TopMLP.fc8': {
        'same_name_events_occured_order_in_code_forward': 0,
        'same_name_events_occured_order_in_code_backward': 0,
    }
}


def generate_schedule(schedule, repeat_for_each_event=50, warmup_iterations=10, outfile='prof.schedule'):
    with open(outfile, 'wb') as f:
        f.write(str(warmup_iterations).encode('ascii', 'ignore'))
        iteration = warmup_iterations + 1
        for layer in schedule:
            for f_or_b_event in schedule[layer].keys():
                for interested_event in schedule[layer][f_or_b_event]:
                    if f_or_b_event == 'forward_events':
                        same_name_events_occured_order_in_code = DRLM_EVENTS[layer]['same_name_events_occured_order_in_code_forward']
                    elif f_or_b_event == 'backward_events':
                        same_name_events_occured_order_in_code = DRLM_EVENTS[layer]['same_name_events_occured_order_in_code_backward']
                    else:
                        raise Exception("{} not a forward_event nor a backward_event".format(interested_event))
                    for _ in range(repeat_for_each_event):
                        line = "\n{} {} {} {}".format(
                            interested_event,
                            iteration,
                            layer,
                            same_name_events_occured_order_in_code
                        ).encode('ascii', 'ignore')
                        f.write(line)
                        iteration += 1

def parse_result(prof_file='prof.json'):
    with open(prof_file, 'r') as f:
        jstring = f.read()
        prof = json.loads(jstring)
        timeline = prof['events']
        timeline.sort(key=lambda e: e["start_index"])
        result = {
            'host_name': prof['host_name'],
            'avg_iter_time_ms': sum(prof['iter_time_ms']) / len(prof['iter_time_ms']),
            'layers': split_by_layer_device_stream(timeline)
        }
    return result

def print_result(result):
    print(json.dumps(result, indent=2))


def split_by_layer_device_stream(timeline):
    # timeline is a array
    global_streams = []
    result = OrderedDict()
    for event in timeline:
        layer_name = event["layer_name"]
        if layer_name not in result.keys():
            result[layer_name] = OrderedDict()
        device_id = "device_" + str(event["device_id"])
        if device_id not in result[layer_name].keys():
            result[layer_name][device_id] = OrderedDict()
        if event["stream"] in global_streams:
            stream_id = "stream_" + str(global_streams.index(event["stream"]))
        else:
            stream_id = "stream_" + str(len(global_streams))
            global_streams.append(event["stream"])
        if stream_id not in result[layer_name][device_id].keys():
            result[layer_name][device_id][stream_id] = []
        new_event = OrderedDict()
        new_event["name"] = event["name"]
        new_event["start_index"] = event["start_index"]
        new_event["end_index"] = event["end_index"]

        #measured_times_ms = reject_outliers(event["measured_times_ms"])
        measured_times_ms = event["measured_times_ms"]
        new_event["avg_measured_time_ms"] = sum(measured_times_ms) / len(measured_times_ms)
        #iter_start_to_event_start_times_ms = reject_outliers(event["iter_start_to_event_start_times_ms"])
        iter_start_to_event_start_times_ms = event["iter_start_to_event_start_times_ms"]
        new_event["avg_iter_start_to_event_start_time_ms"] = sum(iter_start_to_event_start_times_ms) / len(iter_start_to_event_start_times_ms)
        result[layer_name][device_id][stream_id].append(new_event)
    return result

def reject_outliers(data, m=2.):
    data = np.array(data)
    return data[abs(data - np.mean(data)) < m * np.std(data)].tolist()

def draw_barh(result):
    num_of_events = 0
    for l in result["layers"].keys():
        for d in result["layers"][l].keys():
            for s in result["layers"][l][d].keys():
                for _ in result["layers"][l][d][s]:
                    num_of_events += 1

    figure_width = 20
    figure_height = num_of_events * 0.9

    plt.figure(figsize=(figure_width, figure_height))
    plt.title('Profiling Result', fontsize=20)
    plt.xlabel(u'Time Cost in ms', fontsize=14)
    plt.ylabel(u'Events',fontsize=14, loc = 'top')

    width_val = 0.4 #若显示 n 个柱状图，则width_val的值需小于1/n ，否则柱形图会有重合

    y_labels = []
    x_events_data = []

    for l in result["layers"].keys():
        for d in result["layers"][l].keys():
            for s in result["layers"][l][d].keys():
                label = l + '_' + d + '_' + s + '_'
                for e in result["layers"][l][d][s]:
                    label = l + '_' + d + '_' + s + '_' + e["name"]
                    plt.barh(label, e["avg_measured_times_ms"])

    #plt.legend(loc=2)#图例展示位置，数字代表第几象限
    plt.show()#显示图像