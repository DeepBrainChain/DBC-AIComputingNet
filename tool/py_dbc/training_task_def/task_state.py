class task_state(object):
    def __init__(self, task_id, task_state, create_time, code_hash, engine, entry_file):
        self.task_id = task_id
        self.task_state = task_state
        self.create_time = create_time
        self.code_hash = code_hash
        self.engine = engine
        self.entry_file = entry_file