use bevy::{
    prelude::*,
    window::{
        PresentMode, WindowMode, WindowTheme
    },
};

fn max_window(mut commands: Commands, mut window: Single<&mut Window>) {
    commands.spawn(Camera2d);
    window.visible = true;
    window.set_maximized(true);
}

fn main() {
    App::new()
        .add_plugins((
            DefaultPlugins.set(WindowPlugin {
                primary_window: Some(Window {
                    title: "Cellulon Simulation".into(),
                    decorations: true,
                    mode: WindowMode::Windowed,
                    position: WindowPosition::Centered(MonitorSelection::Primary),
                    present_mode: PresentMode::AutoVsync,
                    window_theme: Some(WindowTheme::Dark),
                    ..default()
                }),
                ..default()
            }),
        ))
        .add_systems(Startup, max_window)
        .run();
}