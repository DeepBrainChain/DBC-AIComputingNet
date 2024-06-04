from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import time

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):
  def do_POST(self):
    if self.path == '/api/v1/power':
      # 设置响应头
      self.send_response(200)
      self.send_header("Content-type", "application/json")
      self.end_headers()

      # 读取请求体
      content_length = int(self.headers['Content-Length'])
      post_data = self.rfile.read(content_length)
      request_json = json.loads(post_data)
      print(f'Received HTTP request {request_json}')

      time.sleep(15)

      # 处理请求并生成响应
      response = {
        "mac": request_json['id']
      }

      # 发送响应
      response_json = json.dumps(response)
      self.wfile.write(response_json.encode('utf-8'))
    else:
      # 路径不匹配时返回 404
      self.send_response(404)
      self.send_header("Content-type", "application/json")
      self.end_headers()
      response = {
        "code": 404,
        "message": "Not Found"
      }
      self.wfile.write(json.dumps(response).encode('utf-8'))

def run(server_class=HTTPServer, handler_class=SimpleHTTPRequestHandler, port=8090):
  server_address = ('', port)
  httpd = server_class(server_address, handler_class)
  print(f'Starting HTTP server on port {port}...')
  httpd.serve_forever()

if __name__ == '__main__':
  run()
