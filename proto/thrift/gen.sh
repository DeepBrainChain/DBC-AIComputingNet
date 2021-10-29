#!/bin/bash
thrift --gen cpp -out ./cpp task.thrift
thrift --gen cpp -out ./cpp matrix.thrift
thrift --gen cpp -out ./cpp vm_task_result.thrift
