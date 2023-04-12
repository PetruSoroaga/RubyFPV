#!/bin/bash

./ruby_test_gen_stream >> /tmp/ruby/fifo1&
./ruby_tx_video /tmp/ruby/fifo1