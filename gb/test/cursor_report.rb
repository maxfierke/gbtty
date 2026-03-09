#!/usr/bin/env ruby

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'serialport'
end

gbtty_path = '/dev/tty.usbmodemTEST1'
baud_rate = 115200

SerialPort.open(gbtty_path, baud_rate, 8, 1, SerialPort::NONE) do |sp|
  sp.read_timeout = 1000

  response = "\x00"
  print "Awaiting gbtty readiness"
  while response != "\e[0n"
    print "."
    sp.write("\e[5n")
    sp.flush
    sleep 0.2
    response = sp.read(4)
  end

  puts

  puts "Sending cursor position query (CSI 6n)..."
  sp.write("\e[6n")
  sp.flush
  sleep 0.2

  response = sp.read(100) || ""
  puts "response: #{response.inspect}"

  puts "\nSending device attributes query (CSI c)..."
  sp.write("\e[c")
  sp.flush
  sleep 0.2

  response = sp.read(100) || ""
  puts "response: #{response.inspect}"
end
