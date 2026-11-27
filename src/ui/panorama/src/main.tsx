import { on } from './bridge/events';
import { guarded, probeStyles } from './bridge/log';
import { changerPage } from './features/changer/ChangerPage';
import { startMvpBanner } from './features/changer/mvp';
import { startTagLoop } from './features/changer/tag';
import { render } from './renderer/panorama';
import { addPage } from './shell/pages';
import { Shell } from './shell/Shell';
import { closeTab, createTab } from './shell/tab';

guarded(() => {
  addPage(changerPage);
  const root = OpenChangerAPI.root;
  probeStyles(root);
  const dispose = render(() => <Shell onClose={closeTab} />, root);
  const removeTab = createTab(root);
  const stopTags = startTagLoop();
  const stopMvp = startMvpBanner();
  on('ui.unload', () => {
    removeTab();
    stopTags();
    stopMvp();
    dispose();
  });
})();
