def can_build(env, platform):
    # opendust_agent is optional: without it the Room node still works, it just
    # doesn't register itself with the bridge's room list.
    env.module_add_dependencies("opendust_slate", ["opendust_agent"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "AgentSlate3D",
        "ClaudeSession",
        "Room",
        "SlatePanel",
    ]


def get_doc_path():
    return "doc_classes"
