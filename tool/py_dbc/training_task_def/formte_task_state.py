def formate_task_state(status):
    return {
        1:"task_unknown",
        2:"task_queueing",
        4:"task_running",
        8:"task_stopped",
        16:"task_successfully_closed",
        32:"task_abnormally_closed",
        64: "task_overdue_closed"
    }.get(status, "task_unknown")