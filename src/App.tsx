import {useCallback, useEffect, useState} from 'react';
import {BrowserWorkspace} from './components/BrowserWorkspace';
import {LandingPage} from './components/LandingPage';
import {OfflineIndicator} from './components/OfflineIndicator';

type AppRoute = 'landing' | 'browser';

function resolveRoute(pathname: string): AppRoute {
  return pathname.startsWith('/browser') ? 'browser' : 'landing';
}

export default function App() {
  const [route, setRoute] = useState<AppRoute>(() =>
    typeof window !== 'undefined' ? resolveRoute(window.location.pathname) : 'landing'
  );

  useEffect(() => {
    const handlePopState = () => {
      setRoute(resolveRoute(window.location.pathname));
    };
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  const navigate = useCallback((nextRoute: AppRoute) => {
    const targetPath = nextRoute === 'browser' ? '/browser' : '/';
    if (window.location.pathname !== targetPath) {
      window.history.pushState({}, '', targetPath);
    }
    setRoute(nextRoute);
    window.scrollTo({top: 0});
  }, []);

  return (
    <>
      {route === 'browser' ? (
        <BrowserWorkspace onBackToLanding={() => navigate('landing')} />
      ) : (
        <LandingPage onOpenBrowser={() => navigate('browser')} />
      )}
      <OfflineIndicator />
    </>
  );
}
