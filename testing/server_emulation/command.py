
# Copyright 2021 University of Nottingham Ningbo China
# Author: Filippo Savi <filssavi@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import socket
import msgpack
import json

channel_data_raw = []

def socket_recv(socket, n_bytes):
    raw_data = b''
    while len(raw_data) != n_bytes:
        raw_data += socket.recv(n_bytes - len(raw_data))
    return raw_data


def send_command(obj:dict):
    command = json.dumps(obj)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        data = 0
        s.connect(("localhost", 6666))
        
        raw_command = command.encode()

        command_length = str(len(raw_command)).zfill(10)
        
        s.send(command_length.encode())
        socket_recv(s, 2)
        s.send(raw_command)

        raw_status_resp = socket_recv(s, 4)
        response_length = int.from_bytes(raw_status_resp, "big")
        raw_response = socket_recv(s, response_length)

        resp_obj = msgpack.unpackb(raw_response)
        return resp_obj


args = {
    'type': 'proxied',
    'proxy_address':321,
    'proxy_type':"test",
    'address': 44,
    'value': 63
}

result = send_command({"cmd": 2, "args":args})
print(result)
