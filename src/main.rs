use bevy::{
    diagnostic::{LogDiagnosticsPlugin},
    prelude::*,
    window::{
        PresentMode, WindowMode, WindowResolution, WindowTheme
    },
};

fn max_window(mut window: Query<&mut Window>) {
    let mut window = window.single_mut().unwrap();
    window.set_maximized(true);
    window.position = WindowPosition::Centered(MonitorSelection::Primary);
    error!("WELL, I FREAKING TRIED {}", window.title);
}

fn main() {
    unsafe {
        std::env::set_var("WINIT_UNIX_BACKEND", "x11");
    }
    println!("Back? {}", std::env::var("WINIT_UNIX_BACKEND").unwrap_or_else(|_| "NOT SET".into()));

    App::new()
        .add_plugins((
            DefaultPlugins.set(WindowPlugin {
                primary_window: Some(Window {
                    title: "Cellulon Simulation".into(),
                    decorations: true,
                    mode: WindowMode::Windowed,
                    position: WindowPosition::Centered(MonitorSelection::Primary),
                    resolution: WindowResolution::new(1280, 720).with_scale_factor_override(1.0),
                    present_mode: PresentMode::Fifo,
                    window_theme: Some(WindowTheme::Dark),
                    ..default()
                }),
                ..default()
            }),
            LogDiagnosticsPlugin::default(),
        ))
        .add_systems(Startup, max_window)
        .run();
}
