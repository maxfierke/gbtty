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

  sp.write("\e[1;1H\e[J\e[0m> GBTTY. Connected")
  sp.flush
  sleep 2

  sp.write("\e[4;2HNormal text.")
  sp.flush
  sleep 2

  sp.write("\e[6;2H\e[7mInverted text.\e[27m")
  sp.flush
  sleep 2

  sp.write("\e[12;2HWaiting...")
  sp.flush

  32.times do |i|
    sp.write("\e[8;2HLooped \e[7m#{i}\e[0m times")
    sp.write("\e[12;12H")
    sp.flush
    sleep 0.5
  end

  sleep 0.2

  sp.write("\e[12;2H\e[2KDone.")
  sp.flush

  sleep 0.2

  # Discard some bytes we don't care about
  sp.read(100)

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
