#!/usr/bin/python3
'''
power_manager --poweron '{"uuid":"1c697ac9084e","ipmi_hostname": "192.168.0.110"}'
'''

import argparse
import json
import requests
from sys import exit

http_url = 'http://localhost:8090/api/v1/power'

parser = argparse.ArgumentParser(description='power manager tool')
parser.add_argument('--poweron', help='power/turn on', default=None, metavar='JSON')
parser.add_argument('--poweroff', help='power/turn off', default=None, metavar='JSON')
parser.add_argument('--status', help='query the power status', default=None, metavar='JSON')
args = parser.parse_args()

class PowerHandler:
  mac = ''
  mode = ''

  def __init__(self, mac, mode):
    self.mac = mac
    self.mode = mode

  def handle(self):
    headers = {'Content-Type': 'application/json'}
    json_data = {
      'id': [ self.mac ],
      'mode': self.mode,
      'delay': 0
    }
    try:
      response = requests.post(http_url, headers=headers, json=json_data)
      if self.mac in response.json()['mac']:
        print('success')
        return 0
    except:
      print(f'power {self.mode} failed')
      exit(1)
    print(f'power {self.mode} failed')
    return 1

def parse_mac(jsonstr):
  try:
    device = json.loads(jsonstr)
    return device['uuid']
  except json.JSONDecodeError as e:
    print('parse mac address error')
    exit(1)

if __name__=='__main__':
  mode = None
  jsonstr = None
  if args.poweron is not None:
    mode = 'on'
    jsonstr = args.poweron
  elif args.poweroff is not None:
    mode = 'off'
    jsonstr = args.poweroff
  elif args.status is not None:
    print('Not supported')
    exit(1)
  handler = PowerHandler(parse_mac(jsonstr), mode)
  exit(handler.handle())
