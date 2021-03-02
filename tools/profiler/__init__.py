# Please do not edit this file
import os
import json
import glob
from collections import OrderedDict

#import numpy as np

DRLM_EVENTS = {
    # interested event name
    'High_Level': {
        'same_name_events_occured_order_in_code_forward': 0,
        'same_name_events_occured_order_in_code_backward': 0,
    },
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
    },
    'Loss' : {
        'same_name_events_occured_order_in_code_forward': 0,
    },
    'AllReduce_wgrads' : {
        'same_name_events_occured_order_in_code_forward': 0,
    },
    'Update_Params': {
        'same_name_events_occured_order_in_code_forward': 0,
    }
}

def gen_prof_config(profiling_dir, interested_events=None):
    # create if profiling_dir non-exist.
    os.makedirs(profiling_dir, exist_ok=True)
    if interested_events:
        final_events = {}
        for layer in interested_events:
            for _, events in interested_events[layer].items():
                for e in events:
                    final_events[e] = True
        final_events = final_events.keys()
        with open(os.path.join(profiling_dir, 'prof.events'), 'w') as f:
            f.write("\n".join(final_events))


def sum_result(profiling_dir):
    prof_jsons = glob.glob(os.path.join(profiling_dir, '*.prof.json'))
    ret = []
    for prof_file in prof_jsons:
        with open(prof_file, 'r') as f:
            jstring = f.read()
            prof = json.loads(jstring)
            events = prof['events']
            events.sort(key=lambda e: e["start_index"])
            for e in events:
                e['avg_iter_start_to_event_start_time_ms'] = sum(e['iter_start_to_event_start_times_ms']) / len(e['iter_start_to_event_start_times_ms'])
                e['avg_measured_time_ms'] = sum(e['measured_times_ms']) / len(e['measured_times_ms'])
            result = {
                'host_name': prof['host_name'],
                'avg_iter_time_ms': sum(prof['iter_time_ms']) / len(prof['iter_time_ms']),
                'events': events
            }
            ret.append(result)
    return ret

def parse_result(profiling_dir, interested_events):
    sumed_result = sum_result(profiling_dir)
    for prof in sumed_result:
        prof['events'] = split_by_device_stream_layer_label(prof['events'], interested_events)
    return sumed_result

# def split_by_layer_device_stream(events, interested_events):
#     # events is a array
#     global_streams = []
#     result = OrderedDict()
#     for event in events:
#         layer_name = event["layer_name"]
#         if layer_name not in result.keys():
#             result[layer_name] = OrderedDict()
#         device_id = "device_" + str(event["device_id"])
#         if device_id not in result[layer_name].keys():
#             result[layer_name][device_id] = OrderedDict()
#         if event["stream"] in global_streams:
#             stream_id = "stream_" + str(global_streams.index(event["stream"]))
#         else:
#             stream_id = "stream_" + str(len(global_streams))
#             global_streams.append(event["stream"])
#         if stream_id not in result[layer_name][device_id].keys():
#             result[layer_name][device_id][stream_id] = []
#         new_event = OrderedDict()
#         new_event["name"] = event["name"]
#         new_event["start_index"] = event["start_index"]
#         new_event["end_index"] = event["end_index"]
# 
#         #measured_times_ms = reject_outliers(event["measured_times_ms"])
#         measured_times_ms = event["measured_times_ms"]
#         new_event["avg_measured_time_ms"] = sum(measured_times_ms) / len(measured_times_ms)
#         #iter_start_to_event_start_times_ms = reject_outliers(event["iter_start_to_event_start_times_ms"])
#         iter_start_to_event_start_times_ms = event["iter_start_to_event_start_times_ms"]
#         new_event["avg_iter_start_to_event_start_time_ms"] = sum(iter_start_to_event_start_times_ms) / len(iter_start_to_event_start_times_ms)
#         result[layer_name][device_id][stream_id].append(new_event)
#     return result

def split_by_device_stream_layer_label(events, interested_events):
    global_streams = []
    result = OrderedDict()
    for event in events:
        device_id = "device_" + str(event["device_id"])
        if device_id not in result.keys():
            result[device_id] = OrderedDict()
        if event["stream"] in global_streams:
            stream_id = "stream_" + str(global_streams.index(event["stream"]))
        else:
            stream_id = "stream_" + str(len(global_streams))
            global_streams.append(event["stream"])
        if stream_id not in result[device_id].keys():
            result[device_id][stream_id] = []
        new_event = OrderedDict()
        layer_name = find_layer_name(event, interested_events)
        if not layer_name:
            continue
        new_event["label"] = layer_name + '.' + event['event_name']
        #new_event["start_index"] = event["start_index"]
        #new_event["end_index"] = event["end_index"]
        new_event["measured_times_ms"] = event["measured_times_ms"]
        new_event["iter_start_to_event_start_times_ms"] = event["iter_start_to_event_start_times_ms"]
        new_event["avg_measured_time_ms"] = event["avg_measured_time_ms"]
        new_event["avg_iter_start_to_event_start_time_ms"] = event["avg_iter_start_to_event_start_time_ms"]
        result[device_id][stream_id].append(new_event)
    for _, device_events in result.items():
        for _, stream_events in device_events.items():
            stream_events.sort(key=lambda e: e["avg_iter_start_to_event_start_time_ms"])
    return result

def find_layer_name(event, interested_events):
    for layer in interested_events.keys():
        if event['event_name'] in interested_events[layer]['forward_events'] and \
           event['met_times_within_this_stream'] == DRLM_EVENTS[layer]['same_name_events_occured_order_in_code_forward']:
            return layer
        if 'backward_events' in interested_events[layer].keys():
            if event['event_name'] in interested_events[layer]['backward_events'] and \
               event['met_times_within_this_stream'] == DRLM_EVENTS[layer]['same_name_events_occured_order_in_code_backward']:
               return layer
    return None


#def reject_outliers(data, m=2.):
#    data = np.array(data)
#    return data[abs(data - np.mean(data)) < m * np.std(data)].tolist()
