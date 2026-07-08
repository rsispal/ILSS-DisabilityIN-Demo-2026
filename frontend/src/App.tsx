import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { AppProvider } from './context/AppContext';
import { LanyardSimulatorPage } from './pages/LanyardSimulatorPage';

export default function App() {
  return (
    <AppProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<LanyardSimulatorPage />} />
        </Routes>
      </BrowserRouter>
    </AppProvider>
  );
}
