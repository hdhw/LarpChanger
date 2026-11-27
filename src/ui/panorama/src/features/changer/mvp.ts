import { on } from '../../bridge/events';
import { guarded, log } from '../../bridge/log';

const api = OpenChangerAPI.extras;
// The game fills the banner right after round_mvp, so write over it a few times.
const retries = [0.05, 0.3, 1];

export function kitName(title: string, artist: string) {
  return [artist, title].filter((part) => part !== '').join(', ');
}

function topPanel() {
  let panel: GenericPanel = $.GetContextPanel();
  for (let parent = panel.GetParent(); parent; parent = parent.GetParent()) panel = parent;
  return panel;
}

function dress() {
  const saved = api.settings();
  const icon = topPanel().FindChildTraverse('MVPMusicKitIcon') as Image | null;
  if (!icon?.IsValid()) {
    log('mvp banner not found');
    return;
  }
  const cover = api.mvpIcon();
  if (cover !== '') icon.SetImage(cover);
  const name = kitName(saved.MvpTitle, saved.MvpArtist);
  if (name === '') return;
  for (let panel: GenericPanel | null = icon; panel; panel = panel.GetParent()) {
    panel.SetDialogVariable('winpanel-musickit-name', name);
    if (panel.id === 'WinPanelRoot') break;
  }
}

export function startMvpBanner() {
  return on('extras.mvp', () => {
    for (const delay of retries) $.Schedule(delay, guarded(dress));
  });
}
