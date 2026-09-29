extends CharacterBody3D
## Minimal first-person controller for the slate demo.
## WASD to move, mouse to look (click to capture, Esc releases when the slate is down).
## E raises the slate; while it is raised the keyboard belongs to the slate.

@export var speed := 4.0
@export var mouse_sensitivity := 0.0025
@export var gravity := 9.8

@onready var camera: Camera3D = $Camera3D
@onready var slate: AgentSlate3D = $Camera3D/Slate

var _pitch := 0.0


func _ready() -> void:
	slate.raised_changed.connect(_on_slate_raised_changed)


func _unhandled_input(event: InputEvent) -> void:
	if slate.raised:
		return
	if event.is_action_pressed("mouse_capture"):
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	elif event.is_action_pressed("ui_cancel"):
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	elif event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-event.relative.x * mouse_sensitivity)
		_pitch = clampf(_pitch - event.relative.y * mouse_sensitivity, -1.4, 1.4)
		camera.rotation.x = _pitch


func _physics_process(delta: float) -> void:
	if not is_on_floor():
		velocity.y -= gravity * delta
	var input_dir := Vector2.ZERO
	if not slate.raised:
		input_dir = Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var direction := (transform.basis * Vector3(input_dir.x, 0.0, input_dir.y)).normalized()
	velocity.x = direction.x * speed
	velocity.z = direction.z * speed
	move_and_slide()


func _on_slate_raised_changed(raised: bool) -> void:
	# Free the mouse while typing so the cursor is visible; recapture on lower.
	if raised:
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
