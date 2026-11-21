import { createSignal } from 'solid-js';
import { guarded } from '../bridge/log';
import { t } from '../i18n';
import { back } from './pages';

const hiddenClass = 'mainmenu-content--hidden';
const [tabShown, setTabShown] = createSignal(false);

export { tabShown };

function find(id: string) {
  const panel = $.GetContextPanel().FindChildInLayoutFile(id);
  if (!panel) throw new Error(`the main menu has no ${id}`);
  return panel;
}

function hideWhenFaded(tab: Panel) {
  return (panel: GenericPanel, property: string) => {
    if (panel.id !== tab.id || property !== 'opacity') return false;
    if (tab.visible && tab.BIsTransparent()) {
      tab.SetReadyForDisplay(false);
      tab.visible = false;
      setTabShown(false);
      return true;
    }
    if (tab.visible) {
      $.DispatchEvent('MainMenuTabShown', tab.id);
      setTabShown(true);
    }
    return false;
  };
}

// Star of David from six bars: [centre x, centre y, angle] of each triangle edge in a 26px box.
const starEdges = [
  [13, 19, 0],
  [7.8, 10, -60],
  [18.2, 10, 60],
  [13, 7, 0],
  [7.8, 16, 60],
  [18.2, 16, -60],
] as const;
const edgeLength = 20.8;

function createStar(button: GenericPanel) {
  const box = $.CreatePanel('Panel', button, '', { class: 'mainmenu-top-navbar__radio-btn__icon' });
  box.style.width = '26px';
  box.style.height = '26px';
  box.hittest = false;
  for (const [x, y, angle] of starEdges) {
    const edge = $.CreatePanel('Panel', box, '');
    edge.hittest = false;
    edge.style.width = `${edgeLength}px`;
    edge.style.height = '2px';
    edge.style.backgroundColor = '#ffffff';
    edge.style.position = `${x - edgeLength / 2}px ${y - 1}px 0px`;
    edge.style.transform = `rotateZ(${angle}deg)`;
  }
}

function createButton(tab: Panel, settings: GenericPanel) {
  const bar = settings.GetParent() as Panel;
  const button = $.CreatePanel('RadioButton', bar, '', {
    class: 'mainmenu-top-navbar__radio-iconbtn',
    group: 'NavBar',
  });
  createStar(button);
  const events = {
    onactivate: () => MainMenu.NavigateToTab(tab.id, ''),
    onmouseover: () => UiToolkitAPI.ShowTextTooltipOnPanel(button, t.appName),
    onmouseout: () => UiToolkitAPI.HideTextTooltip(),
  };
  for (const [event, handler] of Object.entries(events)) button.SetPanelEvent(event, guarded(handler));
  bar.MoveChildAfter(button, settings);
  return button;
}

export function closeTab() {
  MainMenu.OnHomeButtonPressed();
}

export function createTab(tab: Panel) {
  const settings = find('MainMenuNavBarSettings');
  tab.SetParent(find('JsMainMenuContent'));
  tab.AddClass('mainmenu-content__container');
  tab.AddClass(hiddenClass);
  tab.hittest = true;
  tab.visible = false;
  tab.SetReadyForDisplay(false);
  tab.RegisterForReadyEvents(true);
  $.RegisterEventHandler('PropertyTransitionEnd', tab, guarded(hideWhenFaded(tab)));
  $.RegisterKeyBind(
    tab,
    'key_escape',
    guarded(() => {
      if (!back()) closeTab();
    }),
  );
  const button = createButton(tab, settings);
  return () => {
    if (!tab.BHasClass(hiddenClass)) closeTab();
    button.DeleteAsync(0);
  };
}
