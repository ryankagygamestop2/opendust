def can_build(env, platform):
    # The bridge needs Godot's WebSocket implementation for its transport.
    env.module_add_dependencies("opendust_agent", ["websocket"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "OpenDustAgentServer",
        "OpenDustToolRegistry",
    ]


def get_doc_path():
    return "doc_classes"
