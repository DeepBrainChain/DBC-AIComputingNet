import web
import model
from manager.p2p_service_manager import *
from manager.connect_manager import *
from manager.dispatcher_manager import  *
from manager.ai_training_request_manager import *
from manager.ai_mining_device_manager import *
from common_def.global_def import *
import time_module
### Url mappings

urls = (
    '/', 'Index',
    '/view/(\d+)', 'View',
    '/new', 'New',
    '/delete/(\d+)', 'Delete',
    '/edit/(\d+)', 'Edit',
    '/submit_task', 'submit_task',
    # '/list_task', 'list_task',
    '/list_p2p_active','list_p2p_active',
    '/list_p2p_candidate','list_p2p_candidate',
    '/list_ai_mining','list_ai_mining',
    '/upload', 'Upload'
)

### Templates
t_globals = {
    'datestr': web.datestr,
    'time_tool':time_module
}
render = web.template.render('templates', base='base', globals=t_globals)


class list_p2p_active:
    def GET(self):
        p2pmanager = get_manager(p2p_service_name)
        posts = p2pmanager.get_p2p_actives()
        return render.list_p2p_active(posts)
class list_p2p_candidate:
    def GET(self):
        p2pmanager = get_manager(p2p_service_name)
        posts = p2pmanager.get_p2p_candidates()
        return render.list_p2p_candidate(posts)
class list_ai_mining:
    device_form = web.form.Form(
        web.form.Dropdown('gpu_model', ""),
        web.form.Textbox("gpu_num",
                         web.form.notnull,
                         web.form.regexp('\d+', 'Must be a digit'),
                         web.form.Validator('Must be more than -1', lambda x: int(x) > -1)),
        web.form.Button('query')
    )
    def pre_form(self):
        ai_ming = get_manager(ai_mining_device_manager_name)
        posts = ai_ming.get_gpu_models()
        if posts.__len__() > 0:
            default_value = posts.pop()
            posts.add(default_value)
            # ss=web.form.Dropdown('gpu_model', posts, value=default_value)
            self.device_form.get("gpu_model").args = posts
            self.device_form.get("gpu_model").value = default_value
    def GET(self):
        ai_ming = get_manager(ai_mining_device_manager_name)
        posts_all = ai_ming.get_ai_mining_devices()
        self.pre_form()
        return render.list_ai_mining(posts_all, self.device_form)
    def POST(self):
        form = self.device_form()
        if not form.validates():
            return render.new(form)
        gpu_model = form.d.gpu_model
        gpu_num = int(form.d.gpu_num)

        # model.new_post(form.d.title, form.d.content)
        ai_ming = get_manager(ai_mining_device_manager_name)
        posts = ai_ming.get_ai_mining_by_condition(gpu_num, gpu_model)
        self.pre_form()
        return render.list_ai_mining(posts, self.device_form)
class submit_task:
    task_form = web.form.Form(
        # web.form.
        web.form.Textbox("code_dir",description="code dir"),
        web.form.Checkbox('code_dir_hash',checked=True),
        web.form.Textbox("peer_nodes", description="peer nodes"),
        web.form.Textbox("entry_file", description="entry_file"),
        web.form.Textbox("engine", description="train engin"),
        web.form.Textbox("hyper_params", description="hyper_params"),
        web.form.Button('submit')
    )
    def GET(self):
        form = self.task_form()
        return render.submit_task(form)
    def POST(self):
        # form = self.task_form()
        # if not form.validates():
        #     return render.submit_task(form)
        x = web.input()
        code_dir_hash= True if x.has_key('code_dir_hash') else False
        code_dir = x["code_dir"]
        peer_nodes = x["peer_nodes"]
        entry_file = x["entry_file"]
        engine = x["engine"]
        hyper_params = x["hyper_params"]
        ai_req = get_manager(ai_power_requestor_service_name)
        # (self, code_dir, peer_nodes, entry_file, engine, hyper_params):
        task = ai_req.cmd_start_train_req(code_dir,peer_nodes,entry_file,engine,hyper_params,code_dir_hash)
        return render.show_submit_result(task)
class list_task:
    def GET(self):
        pass
class Upload:
    def GET(self):
        return render.upload()
    def POST(self):
        x = web.input(myfile={})
        file_name=x['myfile'].filename
        web.debug(x['myfile'].filename)
        web.debug(x['myfile'].value)
        web.debug(x['myfile'].file.read())
        raise web.seeother('/upload')
        # pass


class Index:
    def GET(self):
        """ Show page """
        # posts = model.get_posts()
        # return render.index(posts)
        p2pmanager = get_manager(p2p_service_name)
        posts = p2pmanager.get_p2p_actives()
        return render.list_p2p_active(posts)

class View:
    def GET(self, id):
        """ View single post """
        post = model.get_post(int(id))
        return render.view(post)


class New:
    form = web.form.Form(
        web.form.Textbox('title', web.form.notnull,
                         size=30,
                         description="Post title:"),
        web.form.Textarea('content', web.form.notnull,
                          rows=30, cols=80,
                          description="Post content:"),
        web.form.Button('Post entry'),
    )

    def GET(self):
        form = self.form()
        return render.new(form)

    def POST(self):
        form = self.form()
        if not form.validates():
            return render.new(form)
        model.new_post(form.d.title, form.d.content)
        raise web.seeother('/')


class Delete:

    def POST(self, id):
        model.del_post(int(id))
        raise web.seeother('/')


class Edit:

    def GET(self, id):
        post = model.get_post(int(id))
        form = New.form()
        form.fill(post)
        return render.edit(post, form)

    def POST(self, id):
        form = New.form()
        post = model.get_post(int(id))
        if not form.validates():
            return render.edit(post, form)
        model.update_post(int(id), form.d.title, form.d.content)
        raise web.seeother('/')


app = web.application(urls, globals())

if __name__ == '__main__':
    app.run()